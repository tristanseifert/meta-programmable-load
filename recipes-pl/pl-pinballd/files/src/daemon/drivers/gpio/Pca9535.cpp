#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __linux__
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#endif

#include <array>
#include <system_error>

#include <fmt/format.h>
#include <plog/Log.h>

#include "Pca9535.h"

using namespace Drivers::Gpio;

/**
 * @brief Initialize the IO expander
 *
 * Given an already opened I2C bus, set up the IO expander. This will configure all pins as
 * inputs and disable interrupts.
 */
Pca9535::Pca9535(int fd, const uint8_t address) : bus(fd), busAddress(address) {
    // configure all pins as inputs
    this->cfgPort.value = 0xffff;
    this->inversionPort.value = 0;
    this->outputPort.value = 0;

    this->updatePinConfig();
}

/**
 * @brief Configure a pin
 *
 * Pins can be configured either as an input or output - no additional configuration is available
 * beyond input polarity inversion.
 */
void Pca9535::configurePin(const size_t pin, const PinMode mode) {
    const uint16_t bit = (1U << pin);

    if(mode & PinMode::Input) {
        this->cfgPort.value |= bit;

        // handle inversion
        if(mode & PinMode::Inverted) {
            this->inversionPort.value |= bit;
        } else {
            this->inversionPort.value &= ~bit;
        }
    } else {
        this->cfgPort.value &= ~bit;
    }

    this->updatePinConfig();
}

/**
 * @brief Set the state of a pin
 *
 * Write the output state of a pin.
 */
void Pca9535::setPinState(const size_t pin, bool asserted) {
    const uint16_t bit = (1U << pin);

    if(asserted) {
        this->outputPort.value |= bit;
    } else {
        this->outputPort.value &= ~bit;
    }

    this->updateOutputPort();
}



/**
 * @brief Write a single 8-bit register
 */
void Pca9535::writeReg(const Register reg, const uint8_t value) {
    int err;

    PLOG_DEBUG << fmt::format("<< wr {:02x} = {:02x}", static_cast<uint8_t>(reg), value);

    // select slave
    err = ioctl(this->bus, I2C_SLAVE, this->busAddress);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "Pca9535: set slave address");
    }

    // perform the transaction
    std::array<uint8_t, 2> txd{{static_cast<uint8_t>(reg), value}};
    err = write(this->bus, txd.data(), txd.size());
    if(err != txd.size()) {
        throw std::system_error(errno, std::generic_category(), "Pca9535: write register");
    }
}

