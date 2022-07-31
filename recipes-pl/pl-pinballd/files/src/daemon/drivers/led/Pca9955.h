#ifndef DRIVERS_LED_PCA9955_H
#define DRIVERS_LED_PCA9955_H

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include <uuid.h>

#include "LedManager.h"
#include "drivers/Driver.h"

struct cbor_item_t;

namespace drivers::led {
/**
 * @brief 16-channel constant current LED driver
 *
 * This is a driver for the PCA9955B 16-channel constant current LED driver.
 */
class Pca9955: public DriverBase, public LedManager::DriverInterface,
    public std::enable_shared_from_this<Pca9955> {
    private:
        /// Device register addresses
        enum Register: uint8_t {
            /// Mode register 1
            Mode1                       = 0x00,
            /// Mode register 2
            Mode2                       = 0x01,

            /**
             * @brief LEDOUT0
             *
             * These registers specify the state of the LED driver for a particular channel; each
             * channel receives two bits in the register:
             *
             * - 0b00: LED driver off
             * - 0b01: LED is fully on (PWM not active)
             * - 0b10: LED is controlled by PWM register
             * - 0b11: LED is controlled by PWM and group dimming PWM
             *
             * This register contains data for channels 0..3.
             */
            LEDOUT0                     = 0x02,
            LEDOUT1                     = 0x03,
            LEDOUT2                     = 0x04,
            LEDOUT3                     = 0x05,

            /**
             * @brief Group brightness control
             *
             * Duty cycle for group dimming.
             */
            GroupDutyCycle              = 0x06,
            /**
             * @brief Group blinking control
             *
             * Duty cycle of the blinking period; in steps of 67ms.
             */
            GroupBlinkDutyCycle         = 0x07,

            /**
             * @brief Output PWM duty cycle
             *
             * Contains the duty cycle (and thus, the desired output brightness) of the channel.
             */
            PWM0                        = 0x08,

            /**
             * @brief Output current value
             *
             * Set the reference current for full brightness for the LED. This is a proportion of
             * the current through Rext.
             */
            IREF0                       = 0x18,

            /**
             * @brief Output delay
             *
             * Specifies the delay between PWM edges between channels, in 125ns increments.
             */
            PwmEdgeOffset               = 0x3f,

            /**
             * @brief Set PWM duty cycle for all outputs
             *
             * When writing to this register, _all_ channels' PWM duty cycle will be set to the
             * written value.
             */
            PWMAll                      = 0x44,
        };

    public:
        /// Number of channels in the controller
        constexpr static const size_t kNumChannels{16};
        /// Hardware driver id
        constexpr static const uuids::uuid kDriverId{{
            0xBB, 0x47, 0x0F, 0xB9, 0x19, 0x76, 0x4A, 0xC8, 0x9F, 0x5B, 0x33, 0x66, 0xBD, 0xF8, 0x06, 0x0E,
        }};

        /// Default IREF setting for channels
        constexpr static const uint8_t kDefaultCurrent{0x20};

    public:
        Pca9955(const int busFd, const cbor_item_t *config);
        ~Pca9955();

        void driverDidRegister(Probulator *) override;

        /**
         * @brief Set the global brightness
         *
         * This brightness value affects all output channels.
         */
        inline void setGlobalBrightness(const double brightness) {
            const auto temp = std::clamp(brightness, 0., 1.) * 0xff;
            this->writeRegister(Register::GroupDutyCycle, temp);
        }

        void setBrightness(const size_t channel, const double brightness);

    public:
        bool setIndicatorBrightness(const LedManager::Indicator which,
                const double brightness) override;
        bool setIndicatorColor(const LedManager::Indicator which,
                const LedManager::Color &color) override;

        /// Global brightness support is implemented.
        bool supportsIndicatorGlobalBrightness() const override {
            return true;
        }
        void setIndicatorGlobalBrightness(const double brightness) override;

    private:
        void readConfig(const struct cbor_item_t *);
        void readLedMap(const struct cbor_item_t *);

        /**
         * @brief Calculate the LED current for a given current setting
         *
         * @return Current, in mA
         */
        constexpr inline double CalculateCurrent(const uint8_t irefx) {
            return (900. / static_cast<double>(this->rext)) * (static_cast<double>(irefx) / 4.);
        }

        /**
         * @brief Calculate IREF setting for a given current value
         *
         * If the value cannot be represented exactly, we'll return the nearest value with a
         * lower current.
         *
         * @param current Desired LED current, in mA
         *
         * @return IREFx register value
         */
        constexpr inline uint8_t CalculateIref(const double current) {
            // clamp to max current
            if(current >= CalculateCurrent(0xff)) {
                return 0xff;
            }

            return (current * static_cast<double>(this->rext)) / 225.;
        }

        /**
         * @brief Write a single register
         */
        inline void writeRegister(const uint8_t reg, const uint8_t data) {
            std::array<uint8_t, 1> temp{{data}};
            return this->writeRegister(reg, temp);
        }
        void writeRegister(const uint8_t reg, std::span<const uint8_t> data);

    private:
        /// Output debug logs about the per channel current settings
        constexpr static const bool kLogChannelCurrent{false};
        /// Output debug logs about indicators being set
        constexpr static const bool kLogChanges{false};

        /**
         * @brief Information to drive an indicator
         *
         * This struct holds the indices of all the underlying output channels for a given
         * indicator.
         */
        struct LedInfo {
            /// Indices for the
            std::vector<size_t> indices;
        };

        /// File descriptor for the I2C bus
        int busFd{-1};

        /// Resistance value of the current set resistor (much above 3kΩ is not really useful)
        uint16_t rext{0};
        /// Device bus address
        uint8_t address{0};

        /**
         * @brief Channel current reference setting
         *
         * Contains the value for the IREFx register for the given channel
         */
        std::array<uint8_t, kNumChannels> iref;

        /// Mapping from indicator id -> LED info
        std::unordered_map<LedManager::Indicator, LedInfo> channels;
};
}

#endif
