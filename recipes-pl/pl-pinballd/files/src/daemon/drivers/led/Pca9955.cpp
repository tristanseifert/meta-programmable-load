#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <cbor.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "LedManager.h"
#include "Probulator.h"
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

            if(kLogChannelCurrent) {
                PLOG_VERBOSE << fmt::format("{} = {} mA = ${:02x}", i, current, iref);
            }

            this->iref[i] = iref;
        }
    }
    // no current array is specified, so use defaults
    else {
        std::fill(this->iref.begin(), this->iref.end(), kDefaultCurrent);

        const auto defaultCurrent = CalculateCurrent(kDefaultCurrent);
        PLOG_WARNING << fmt::format("Current array missing or invalid, using default ({} mA)",
                defaultCurrent);
    }

    // read the mapping from outputs -> LED type
    auto ledMap = Util::CborMapGet(map, "map");
    if(ledMap && cbor_isa_map(ledMap)) {
        const auto numEntries = cbor_map_size(ledMap);
        if(numEntries > kNumChannels) {
            throw std::runtime_error(fmt::format("map too large ({} entries)", numEntries));
        }

        this->readLedMap(ledMap);
    } else {
        throw std::runtime_error("missing or invalid map");
    }
}

/**
 * @brief Parse the LED arrangement map
 *
 * This is a CBOR map, whose keys are unsigned integers corresponding to a value in the
 * LedManager::Indicator enum. Each entry in the map corresponds to one of the following cases:
 *
 * - `null`: The indicator is not used. This is equivalent to not specifying the indicator at all.
 * - uint: A single channel physical number (for a single color channel)
 * - array: An array of uint channel numbers (up to 3) for a multi-color indicator
 *
 * @seeAlso LedManager::Indicator
 */
void Pca9955::readLedMap(const struct cbor_item_t *ledMap) {
    std::bitset<kNumChannels> allocated;

    auto keys = cbor_map_handle(ledMap);
    const auto numKeys = cbor_map_size(ledMap);

    for(size_t i = 0; i < numKeys; i++) {
        auto &pair = keys[i];
        const auto key = Util::CborReadUint(pair.key);

        // ensure this is a valid key
        if(!LedManager::IsValidIndicatorValue(key)) {
            throw std::runtime_error(fmt::format("invalid LED map key (${:x})", key));
        }

        const auto indicatorId = static_cast<LedManager::Indicator>(key);

        // decode the value
        LedInfo info;

        if(cbor_isa_uint(pair.value)) {
            const auto idx = Util::CborReadUint(pair.value);
            if(allocated[idx]) {
                throw std::runtime_error(fmt::format("already allocated channel {}", idx));
            }

            info.indices.emplace_back(idx);
            allocated[idx] = true;
        } else if(cbor_isa_array(pair.value)) {
            // iterate over all indices
            const auto numChannels = cbor_array_size(pair.value);
            if(numChannels > 3) {
                throw std::runtime_error(fmt::format("too many channels specified ({}, max 3)",
                            numChannels));
            }

            for(size_t j = 0; j < numChannels; j++) {
                const auto idx = Util::CborReadUint(cbor_array_get(pair.value, j));

                if(allocated[idx]) {
                    throw std::runtime_error(fmt::format("already allocated channel {}", idx));
                }

                info.indices.emplace_back(idx);
                allocated[idx] = true;
            }
        } else if(cbor_isa_float_ctrl(pair.value) && cbor_is_null(pair.value)) {
            // ignore it as if it weren't here
            continue;
        } else {
            throw std::runtime_error("invalid LED map value (expected uint, array, or null)");
        }

        // insert the info
        this->channels.emplace(indicatorId, info);
    }

    PLOG_DEBUG << "Assigned channels: " << allocated;
}



/**
 * @brief Register the driver with the LED manager
 */
void Pca9955::driverDidRegister(Probulator *probulator) {
    probulator->getLedManager()->registerDriver(this->shared_from_this());
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



/**
 * @brief Set the brightness of an indicator, by its id
 *
 * All channels corresponding to this indicator will be set to the same brightness. Prefer the
 * setIndicatorColor function for multichannel indicators.
 *
 * @seeAlso setIndicatorColor
 */
bool Pca9955::setIndicatorBrightness(const LedManager::Indicator which, const double brightness) {
    using namespace std::placeholders;

    const double temp = std::clamp(brightness, 0., 1.);
    PLOG_INFO << fmt::format("set led {} to {}", (size_t) which, temp);

    // ensure we have this channel
    if(!this->channels.contains(which)) {
        return false;
    }
    const auto &info = this->channels.at(which);

    // set each channel's brightness
    std::for_each(info.indices.begin(), info.indices.end(),
            std::bind(std::mem_fn(&Pca9955::setBrightness), this, _1, brightness));

    return true;
}

/**
 * @brief Set the color of a multicolor indicator
 *
 * Any color components beyond what we have are ignored.
 */
bool Pca9955::setIndicatorColor(const LedManager::Indicator which, const LedManager::Color &color) {
    const auto [cR, cG, cB] = color;
    PLOG_INFO << fmt::format("set led {} to ({}, {}, {})", (size_t) which, cR, cG, cB);

    // ensure we have this channel
    if(!this->channels.contains(which)) {
        return false;
    }
    const auto &info = this->channels.at(which);

    switch(info.indices.size()) {
        case 3:
            this->setBrightness(info.indices[2], cB);
            [[fallthrough]];
        case 2:
            this->setBrightness(info.indices[1], cG);
            [[fallthrough]];
        case 1:
            this->setBrightness(info.indices[0], cR);
            break;

        default:
            throw std::runtime_error(fmt::format("invalid channel info (has {} indices)",
                        info.indices.size()));
    }

    return true;
}

/**
 * @brief Set the global brightness
 *
 * This sets the group duty cycle register.
 */
void Pca9955::setIndicatorGlobalBrightness(const double brightness) {
    const double temp = std::clamp(brightness, 0., 1.);
    const uint8_t duty = static_cast<double>(0xff) * temp;

    this->writeRegister(Register::GroupDutyCycle, duty);
}
