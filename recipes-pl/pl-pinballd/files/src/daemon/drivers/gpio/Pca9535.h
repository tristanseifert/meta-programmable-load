#ifndef DRIVERS_GPIO_PCA9535_H
#define DRIVERS_GPIO_PCA9535_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>

#include "GpioChip.h"

namespace Drivers::Gpio {
/**
 * @brief NXP PCA9535 16-bit I²C GPIO expander
 *
 * This provides a basic driver for the PCA9535 IO expander, with basic support for the interrupt
 * driven mode of operation (by observing a specified system hardware gpio pin)
 */
class Pca9535: public GpioChip {
    public:
        Pca9535(int busFd, const uint8_t address);
        virtual ~Pca9535() = default;

        void configurePin(const size_t pin, const PinMode mode) override;
        void setPinState(const size_t pin, bool asserted) override;
        uint32_t getPinState() override { return 0; };

    private:
        /// Are register reads dumped to the terminal?
        constexpr static const bool kLogRegRead{false};
        /// Are register writes dumped to the terminal?
        constexpr static const bool kLogRegWrite{false};

        /// Register names and offsets for in the chip
        enum class Register: uint8_t {
            Input0                      = 0x00,
            Input1                      = 0x01,
            Output0                     = 0x02,
            Output1                     = 0x03,
            Polarity0                   = 0x04,
            Polarity1                   = 0x05,
            Config0                     = 0x06,
            Config1                     = 0x07,
        };

        /// Structure wrapping combined low/high registers, plus a shadow value
        struct RegData {
            /// Current (desired) value
            uint16_t value{0};
            /// Last value written to the device (if any)
            std::optional<uint16_t> last;
        };

        void writeReg(const Register reg, const uint8_t value);
        // Write a whole 16-bit register. Assumes the passed register name is for the low half.
        void writeReg(const Register reg, const uint16_t value) {
            // TODO: make this optimized for writing the whole register as one transaction
            this->writeReg(reg, static_cast<uint8_t>(value & 0xFF));
            this->writeReg(static_cast<Register>(static_cast<uint8_t>(reg) + 1),
                    static_cast<uint8_t>((value & 0xFF00) >> 8));
        }
        uint8_t readReg(const Register reg);


        /**
         * @brief Update the changed parts of a two-part register
         *
         * This checks which of the two halves of the specified RegData struct changed, then
         * updates the register as needed.
         *
         * @tparam reg Register to update (low half)
         *
         * @param regData Register data structure
         */
        template<Register reg>
        inline void updatePartialRegister(RegData &regData) {
            // get the upper register name
            const auto upperReg = static_cast<Register>(static_cast<uint8_t>(reg) + 1);

            // determine which halves changed
            bool lowerDirty{false}, upperDirty{false};
            if(!regData.last) {
                lowerDirty = true;
                upperDirty = true;
            } else {
                const auto last = *regData.last;
                lowerDirty = (last & 0x00FF) != (regData.value & 0x00FF);
                upperDirty = (last & 0xFF00) != (regData.value & 0xFF00);
            }

            // write 'em out'
            if(lowerDirty && upperDirty) {
                this->writeReg(reg, regData.value);
            } else {
                if(lowerDirty) {
                    this->writeReg(reg, static_cast<uint8_t>(regData.value & 0xFF));
                } else if(upperDirty) {
                    this->writeReg(upperReg, static_cast<uint8_t>((regData.value & 0xFF00) >> 8));
                }
            }

            regData.last = regData.value;
        }

        void updateOutputPort() {
            updatePartialRegister<Register::Output0>(this->outputPort);
        }
        /**
         * @brief Update the inversion port register
         */
        void updateInversionPort() {
            updatePartialRegister<Register::Polarity0>(this->inversionPort);
        }
        /**
         * @brief Update the value of the config port register
         */
        void updateConfigPort() {
            updatePartialRegister<Register::Config0>(this->cfgPort);
        }

        /// Update the configuration of IO pins (mode + polarity)
        void updatePinConfig() {
            this->updateInversionPort();
            this->updateConfigPort();
        }

    private:
        /// File descriptor to the I2C bus
        int bus;
        /// Device address on the bus
        uint8_t busAddress;

        /// Output port register
        RegData outputPort;
        /// Polarity inversion register
        RegData inversionPort;
        /// Configuration port register
        RegData cfgPort;
};
}

#endif
