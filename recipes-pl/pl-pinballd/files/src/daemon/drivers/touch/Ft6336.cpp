#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <cbor.h>
#include <event2/event.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "EventLoop.h"
#include "Utils/Cbor.h"
#include "Ft6336.h"

using namespace drivers::touch;

/**
 * @brief Initialize touch controller driver
 *
 * This parses the specified configuration data to set up the controller.
 *
 * @param busFd Opened file descriptor for the I²C bus
 * @param config Driver configuration payload (should be a map)
 */
Ft6336::Ft6336(const int busFd, const cbor_item_t *config) : busFd(busFd) {
    // validate the config entry as being a map and then read the config
    if(!cbor_isa_map(config)) {
        throw std::runtime_error("invalid config (expected map)");
    }

    this->readConfig(config);
    PLOG_DEBUG << fmt::format("Ft6336: addr ${:02x}, with {}", this->address,
            this->irqEnabled ? "interrupt" : "polling");
    PLOG_DEBUG << fmt::format("Touch array size: {}x{} (rotation {})", this->size.first,
            this->size.second, static_cast<const uint8_t>(this->rotation));

    if(!this->address) {
        throw std::runtime_error("failed to get device address");
    } else if(this->rotation && (!this->size.first || !this->size.second)) {
        throw std::runtime_error("rotation specified without size");
    }

    // read the device version
    this->firmwareVersion = this->readRegister(Register::FirmwareVersion);
    const auto manufacturer = this->readRegister(Register::ManufacturerID);

    PLOG_DEBUG << fmt::format("Manufacturer ${:02x}, fw version ${:02x}", manufacturer,
            this->firmwareVersion);

    // set up interrupt handling or polling
    if(!this->irqEnabled) {
        this->initPollingTimer();
    } else {
        // TODO: implement this
        throw std::runtime_error("irq support not yet implemented");
    }
}

/**
 * @brief Parse the driver's configuration data
 *
 * This is a CBOR map with the following keys:
 *
 * - addr: Bus address for the touch controller
 * - irq: Whether the controller supports interrupts; set to `false` to use timer-driven polling
 * - size: Size of the the underlying display for touch coordinate conversion
 * - rotation: Degrees rotation of the touch panel/display, in 90° increments
 *
 * @note The `size` key is only necessary if touch events should be translated by rotation before
 *       being output.
 */
void Ft6336::readConfig(const cbor_item_t *inConfig) {
    auto entries = cbor_map_handle(inConfig);
    const auto numEntries = cbor_map_size(inConfig);

    for(size_t i = 0; i < numEntries; i++) {
        auto &pair = entries[i];

        // keys should be string
        if(!cbor_isa_string(pair.key) || !cbor_string_is_definite(pair.key)) {
            throw std::runtime_error("invalid key (expected definite string)");
        }

        const auto keyStr = reinterpret_cast<const char *>(cbor_string_handle(pair.key));
        const auto keyStrLen = cbor_string_length(pair.key);

        // process keys
        if(!strncmp("addr", keyStr, keyStrLen)) {
            if(!cbor_isa_uint(pair.value)) {
                throw std::runtime_error("invalid address (expected uint)");
            }

            this->address = Util::CborReadUint(pair.value);
        }
        else if(!strncmp("irq", keyStr, keyStrLen)) {
            // boolean flag?
            if(cbor_float_ctrl_is_ctrl(pair.value)) {
                this->irqEnabled = cbor_get_bool(pair.value);
            }
            else {
                // TODO: implement irq support
                throw std::runtime_error("interrupt support not yet implemented");
            }
        }
        else if(!strncmp("size", keyStr, keyStrLen)) {
            if(!cbor_isa_array(pair.value) || cbor_array_is_indefinite(pair.value)) {
                throw std::runtime_error("invalid size (expected definite array)");
            } else if(cbor_array_size(pair.value) != 2) {
                throw std::runtime_error(fmt::format("invalid size array (got {} elements)",
                            cbor_array_size(pair.value)));
            }

            const uint16_t w = Util::CborReadUint(cbor_array_get(pair.value, 0)),
                  h = Util::CborReadUint(cbor_array_get(pair.value, 1));
            this->size = {w, h};
        }
        else if(!strncmp("rotation", keyStr, keyStrLen)) {
            if(!cbor_isa_uint(pair.value)) {
                throw std::runtime_error("invalid rotation key (expected uint)");
            }

            this->rotation = ((Util::CborReadUint(pair.value) % 360) / 90) & 0x03;
        } else {
            throw std::runtime_error(fmt::format("unknown config key '{}'", keyStr));
        }
    }
}

/**
 * @brief Initialize the polling timer event
 *
 * This fires periodically to query the controller's current touch state.
 */
void Ft6336::initPollingTimer() {
    this->pollingTimer = event_new(EventLoop::Current()->getEvBase(), -1, EV_PERSIST,
            [](auto, auto, auto ctx) {
        reinterpret_cast<Ft6336 *>(ctx)->updateTouchState();
    }, this);
    if(!this->pollingTimer) {
        throw std::runtime_error("failed to allocate watchdog event");
    }

    struct timeval tv{
        .tv_sec  = static_cast<time_t>(kPollInterval / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(kPollInterval % 1'000'000U),
    };

    evtimer_add(this->pollingTimer, &tv);
}

/**
 * @brief Clean up resources associated with the controller
 */
Ft6336::~Ft6336() {
    if(this->pollingTimer) {
        event_free(this->pollingTimer);
    }
}



/**
 * @brief Perform a sequential read of registers at the given starting location
 *
 * Reads one or more registers from the device in a single transaction.
 *
 * @param start First register to read
 * @param outBuffer Buffer to receive the register data (in sequential order)
 * @param numRegs Total number of registers to read, or 0 to use the length of the output buffer
 *
 * @throws std::system_error IO error
 */
void Ft6336::readRegisters(const Register start, std::span<uint8_t> outBuffer,
        const size_t numRegs) {
    std::array<struct i2c_msg, 2> msgs;
    for(auto &msg : msgs) {
        memset(&msg, 0, sizeof(msg));
        msg.addr = this->address;
    }

    std::array<uint8_t, 1> readAddr{{static_cast<uint8_t>(start)}};
    msgs[0].len = readAddr.size();
    msgs[0].buf = readAddr.data();

    msgs[1].flags = I2C_M_RD;
    msgs[1].len = numRegs ? std::min(numRegs, outBuffer.size()) : outBuffer.size();
    msgs[1].buf = outBuffer.data();

    struct i2c_rdwr_ioctl_data txns;
    txns.msgs = msgs.data();
    txns.nmsgs = msgs.size();

    int err = ioctl(this->busFd, I2C_RDWR, &txns);
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "read register");
    }
}

/**
 * @brief Write a device register
 *
 * @param reg Device register address
 * @param value Data to write to the register
 *
 * @throws std::system_error IO error
 */
void Ft6336::writeRegister(const Register reg, const uint8_t value) {
    int err;

    // build up the buffer to send
    std::array<uint8_t, 2> msg{{
        static_cast<uint8_t>(reg), value
    }};

    // do it
    err = ioctl(this->busFd, I2C_SLAVE, this->address);
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "select device");
    }

    err = write(this->busFd, msg.data(), msg.size());
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "write register");
    }
}



/**
 * @brief Read touch controller state
 *
 * Read out the current state of the touch controller, including the location of a touch.
 *
 * This will read the entire bank of registers for touches (starting at P1_Xh, address 0x03; up to
 * P2_MISC at 0x0E) in one bus transaction to reduce the time needed. We'll also optimize this to
 * read only registers for the number of touches that actually are here.
 */
void Ft6336::updateTouchState() {
    std::array<uint8_t, 12> buffer;
    std::fill(buffer.begin(), buffer.end(), 0);

    // get the number of active touch points
    const auto numPoints = this->readRegister(Register::TouchStatus) & 0x0f;

    if(!numPoints) {
        bool changed{false};

        if(this->p1HasData) {
            changed = true;
            this->clearTouchPoint(0);
        }
        if(this->p2HasData) {
            changed = true;
            this->clearTouchPoint(1);
        }

        if(changed) {
            this->sendTouchStateUpdate();
        }
        return;
    } else if(numPoints > 2) {
        throw std::runtime_error(fmt::format("invalid number of touch points: {}", numPoints));
    }

    // read the touch point data
    this->readRegisters(Register::Point1XHigh, buffer, (numPoints == 2) ? 12 : 6);

    // process touch events
    std::span<const uint8_t> regData = buffer;

    this->decodeTouchPoint(0, regData.subspan<0, 6>());

    if(numPoints == 2) {
        this->decodeTouchPoint(1, regData.subspan<6, 6>());
    } else {
        if(this->p2HasData) {
            this->clearTouchPoint(1);
        }
    }

    // secrete a touch data update message
    this->sendTouchStateUpdate();
}

/**
 * @brief Clear cached data for a touch point.
 *
 * Invoked whenever the specified touch point does not have any valid available data.
 */
void Ft6336::clearTouchPoint(const size_t point) {
    if(point > 1) {
        throw std::invalid_argument("invalid point value");
    }

    // clear 'has data' flag
    const auto idx = (this->touchIds[0] == point) ? 0 : 1;

    if(idx == 0) {
        this->p1HasData = false;
    } else if(idx == 1) {
        this->p2HasData = false;
    }

    // clear data itself
    this->touchIds[idx] = 0xff;
}

/**
 * @brief Decode a touch point's register data
 *
 * Parse the register data read from the touch controller and synthesize touch events from it.
 */
void Ft6336::decodeTouchPoint(const size_t point, std::span<const uint8_t, 6> regData) {
    if(point > 1) {
        throw std::invalid_argument("invalid point value");
    }

    // extract register data
    const auto eventType = (regData[0] & 0xc0) >> 6; (void) eventType;
    const auto touchId = (regData[2] & 0xf0) >> 4;
    TouchPosition pos{
        (static_cast<uint16_t>(regData[0] & 0xf) << 8) | static_cast<uint16_t>(regData[1]),
        (static_cast<uint16_t>(regData[2] & 0xf) << 8) | static_cast<uint16_t>(regData[3]),
    };

    if(this->rotation) {
        this->transformTouchPosition(pos);
    }

    // store the touch position (based on touch id)
    this->touchIds[touchId] = point;
    this->touchPositions[point] = pos;

    if(point == 0) {
        this->p1HasData = true;
    } else if(point == 1) {
        this->p2HasData = true;
    }
}

/**
 * @brief Send a touch position update message
 *
 * Notify all connected clients that the touch positions have changed.
 */
void Ft6336::sendTouchStateUpdate() {
    if(this->p1HasData && this->touchIds[0] != 0xff) {
        const auto &pt = this->touchPositions.at(this->touchIds[0]);
        PLOG_DEBUG << fmt::format("Touch 1: ({}, {})", pt.first, pt.second);
    }
    if(this->p2HasData && this->touchIds[1] != 0xff) {
        const auto &pt = this->touchPositions.at(this->touchIds[1]);
        PLOG_DEBUG << fmt::format("Touch 2: ({}, {})", pt.first, pt.second);
    }
}
