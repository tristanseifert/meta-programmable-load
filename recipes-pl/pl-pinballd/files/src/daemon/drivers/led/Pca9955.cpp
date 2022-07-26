#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>

#include <cbor.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "Utils/Cbor.h"
#include "Pca9955.h"

using namespace drivers::led;

/**
 * @brief Initialize LED controller driver
 *
 * This parses the specified configuration data to set up the controller.
 *
 * @param busFd Opened file descriptor for the I²C bus
 * @param config Driver configuration payload (should be a map)
 */
Pca9955::Pca9955(const int busFd, const cbor_item_t *config) : busFd(busFd) {
    // validate the config entry as being a map and then read the config
    if(!cbor_isa_map(config)) {
        throw std::runtime_error("invalid config (expected map)");
    }

    this->readConfig(config);

    PLOG_DEBUG << fmt::format("Pca9955: addr ${:02x}, Rext = {}Ω", this->address, this->rext);

    /*
     * Set up the chip's mode registers: clear errors, use exponential gradation control;
     * disable sub-addresses and all-call addresses and enable auto-increment in the normal mode.
     * Also upload the calculated current reference values.
     */
    // MODE1: disable all auxiliary addresses; use address autoincrement
    this->writeRegister(Register::Mode1, 0b1'00'0'000'0);
    // MODE2: group dimming, exponential gradation
    this->writeRegister(Register::Mode2, 0b00'0'1'0'1'01);
    // 1.5µS between pwm edges
    this->writeRegister(Register::PwmEdgeOffset, 12);
    // set global dimming control to full
    this->setGlobalBrightness(1.);

    // set the current values
    this->writeRegister(Register::IREF0, this->iref);

    /*
     * Ensure each channel's output is disabled. We do this first by setting all channels' PWM
     * duty cycle to zero, then enabling the drivers in individual/group dimming mode.
     */
    this->writeRegister(Register::PWMAll, 0x00);

    std::array<uint8_t, 4> ledoutData{{
        0b11'11'11'11,
        0b11'11'11'11,
        0b11'11'11'11,
        0b11'11'11'11,
    }};
    this->writeRegister(Register::LEDOUT0, ledoutData);
}

/**
 * @brief Clean up the LED driver
 *
 * This will turn off all LEDs.
 */
Pca9955::~Pca9955() {
    this->writeRegister(Register::PWMAll, 0x00);
}

/**
 * @brief Parse the driver's config payload
 *
 * Decode the config payload, which is a CBOR map. It should have the following keys, all of which
 * are required:
 *
 * - `addr`: Bus address of the device (set via the ADDRx pins; not a sub-address!)
 * - `rext`: Value of the external current setting resistor, in Ω
 * - `current`: Array of current values for each of the 16 output channels. Each value is specified
 *   in mA as a floating point quantity.
 * - `map`: Mapping of channels to system LED indicator type
 *
 * @param map CBOR map containing the driver's config
 */
void Pca9955::readConfig(const struct cbor_item_t *map) {
    // first get the device address and resistor value
    if(auto addr = Util::CborMapGet(map, "addr")) {
        this->address = Util::CborReadUint(addr);
    } else {
        throw std::runtime_error("missing device address key");
    }

    if(auto rext = Util::CborMapGet(map, "rext")) {
        this->rext = Util::CborReadUint(rext);
    } else {
        throw std::runtime_error("missing Rext key");
    }

    // parse the current array
    auto currentArray = Util::CborMapGet(map, "current");
    if(currentArray && cbor_isa_array(currentArray)) {
        std::fill(this->iref.begin(), this->iref.end(), 0);

        const auto numEntries = cbor_array_size(currentArray);
        if(numEntries > kNumChannels) {
            throw std::runtime_error(fmt::format("current array too large ({} entries)",
                        numEntries));
        }

        for(size_t i = 0; i < numEntries; i++) {
            const auto value = cbor_array_get(currentArray, i);
            if(!cbor_isa_float_ctrl(value)) {
                PLOG_VERBOSE << fmt::format("type is {}", cbor_typeof(value));
                throw std::runtime_error("invalid current value (expected float)");
            }

            const auto current = cbor_float_get_float(value);
            const auto iref = CalculateIref(current);
            PLOG_VERBOSE << fmt::format("{} = {} mA = ${:02x}", i, current, iref);

            this->iref[i] = iref;
        }

        PLOG_DEBUG << fmt::format("Read {} channels' current data", numEntries);
    }
    // no current array is specified, so use defaults
    else {
        std::fill(this->iref.begin(), this->iref.end(), kDefaultCurrent);

        const auto defaultCurrent = CalculateCurrent(kDefaultCurrent);
        PLOG_WARNING << fmt::format("Current array missing or invalid, using default ({} mA)",
                defaultCurrent);
    }

    // read the mapping from outputs -> LED type
    auto mapArray = Util::CborMapGet(map, "map");
    if(mapArray && cbor_isa_array(mapArray)) {
        const auto numEntries = cbor_array_size(mapArray);
        if(numEntries > kNumChannels) {
            throw std::runtime_error(fmt::format("map array too large ({} entries)", numEntries));
        }

        // TODO: implement
    } else {
        throw std::runtime_error("missing or invalid map array");
    }
}



/**
 * @brief Set the brightness of the specified channel
 *
 * @param channel Channel number ([0, 15])
 * @param brightness New brightness value ([0, 1])
 */
void Pca9955::setBrightness(const size_t channel, const double brightness) {
    if(channel >= kNumChannels) {
        throw std::invalid_argument("invalid channel number");
    }

    const auto temp = std::clamp(brightness, 0., 1.) * 0xff;
    this->writeRegister(Register::PWM0 + channel, static_cast<uint8_t>(temp));
}



/**
 * @brief Write device registers
 *
 * @param start Starting register address
 * @param data One or more bytes of data to write to the device
 */
void Pca9955::writeRegister(const uint8_t start, std::span<const uint8_t> data) {
    int err;

    if(data.empty()) {
        throw std::invalid_argument("data must be at least 1 byte");
    }

    // build up the buffer to send
    std::vector<uint8_t> msg;
    msg.resize(1 + data.size());

    // set the autoincrement bit with the register number
    msg[0] = (start & 0x7f) | ((data.size() > 1) ? 0x80 : 0x00);
    std::copy(data.begin(), data.end(), msg.begin() + 1);

    // send it
    err = ioctl(this->busFd, I2C_SLAVE, this->address);
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "select device");
    }

    err = write(this->busFd, msg.data(), msg.size());
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "write register");
    }
}
