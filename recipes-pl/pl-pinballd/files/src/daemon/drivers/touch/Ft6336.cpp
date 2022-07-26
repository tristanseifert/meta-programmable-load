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
#include "RpcTypes.h"
#include "Rpc/Server.h"
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
    // get address
    if(auto addr = Util::CborMapGet(inConfig, "addr")) {
        this->address = Util::CborReadUint(addr);
    } else {
        throw std::runtime_error("missing or invalid address key");
    }

    // interrupt config
    if(auto irq = Util::CborMapGet(inConfig, "irq")) {
        // boolean flag?
        if(cbor_float_ctrl_is_ctrl(irq)) {
            this->irqEnabled = cbor_get_bool(irq);
        }
        else {
            // TODO: implement irq support
            throw std::runtime_error("interrupt support not yet implemented");
        }
    }

    // panel size
    if(auto size = Util::CborMapGet(inConfig, "size")) {
        if(!cbor_isa_array(size) || cbor_array_is_indefinite(size)) {
            throw std::runtime_error("invalid size (expected definite array)");
        } else if(cbor_array_size(size) != 2) {
            throw std::runtime_error(fmt::format("invalid size array (got {} elements)",
                        cbor_array_size(size)));
        }

        const uint16_t w = Util::CborReadUint(cbor_array_get(size, 0)),
              h = Util::CborReadUint(cbor_array_get(size, 1));
        this->size = {w, h};
    } else {
        throw std::runtime_error("missing panel size");
    }

    // rotation
    if(auto rot = Util::CborMapGet(inConfig, "rotation")) {
        if(!cbor_isa_uint(rot)) {
            throw std::runtime_error("invalid rotation key (expected uint)");
        }

        this->rotation = ((Util::CborReadUint(rot) % 360) / 90) & 0x03;
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
    cbor_item_t *temp{nullptr};

    /*
     * Serialize the touch event as a CBOR map. This contains two keys; type (which is the string
     * "touch") and "touchData" which is in turn another map, where each key is a touch index.
     *
     * Values in the inner map are either `null` if there's no valid data for this touch, or yet
     * another map. This innermost map can have the key "position" which is an array containing
     * the touch position x/y tuple.
     */
    auto root = cbor_new_definite_map(2);
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("type")),
        .value = cbor_move(cbor_build_string("touch"))
    });

    auto touches = cbor_new_definite_map(2);

    // touch point 1
    if(this->p1HasData && this->touchIds[0] != 0xff) {
        const auto &pt = this->touchPositions.at(this->touchIds[0]);
        // PLOG_DEBUG << fmt::format("Touch 1: ({}, {})", pt.first, pt.second);
        temp = this->encodeTouchState(pt);
    } else {
        temp = cbor_new_null();
    }
    cbor_map_add(touches, (struct cbor_pair) {
        .key = cbor_move(cbor_build_uint8(0)),
        .value = cbor_move(temp),
    });

    // touch point 2
    if(this->p2HasData && this->touchIds[1] != 0xff) {
        const auto &pt = this->touchPositions.at(this->touchIds[1]);
        // PLOG_DEBUG << fmt::format("Touch 2: ({}, {})", pt.first, pt.second);
        temp = this->encodeTouchState(pt);
    } else {
        temp = cbor_new_null();
    }
    cbor_map_add(touches, (struct cbor_pair) {
        .key = cbor_move(cbor_build_uint8(1)),
        .value = cbor_move(temp),
    });

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("touchData")),
        .value = cbor_move(touches)
    });

    // serialize the CBOR structure
    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);
    cbor_decref(&root);

    // now broadcast this packet
    try {
        EventLoop::Current()->getRpcServer()->broadcastRaw(Rpc::BroadcastType::TouchEvent,
                kRpcEndpointUiEvent, {reinterpret_cast<std::byte *>(rootBuf), serializedBytes});
        free(rootBuf);
    } catch(const std::exception &) {
        free(rootBuf);
        throw;
    }
}

/**
 * @brief Encode data for a single touch point
 */
struct cbor_item_t *Ft6336::encodeTouchState(const TouchPosition &pos) {
    auto map = cbor_new_definite_map(1);

    // make position array
    auto posArray = cbor_new_definite_array(2);
    cbor_array_push(posArray, cbor_move(cbor_build_uint16(pos.first)));
    cbor_array_push(posArray, cbor_move(cbor_build_uint16(pos.second)));

    cbor_map_add(map, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("position")),
        .value = cbor_move(posArray)
    });

    return map;
}

