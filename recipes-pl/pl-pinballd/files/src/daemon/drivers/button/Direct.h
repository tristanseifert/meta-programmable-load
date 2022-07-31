#ifndef DRIVERS_BUTTON_DIRECT_H
#define DRIVERS_BUTTON_DIRECT_H

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include <uuid.h>

#include "drivers/Driver.h"
#include "drivers/button/Types.h"
#include "drivers/gpio/GpioChip.h"

namespace drivers::button {
/**
 * @brief Directly connected button driver
 *
 * This is a small wrapper around the IO expander driver, which polls the button press state.
 */
class Direct: public DriverBase, public std::enable_shared_from_this<Direct> {
    public:
        /**
         * @brief Set up the direct button io driver
         */
        Direct(const std::shared_ptr<drivers::gpio::GpioChip> &gpio) : gpio(gpio) {
            this->initGpio();
            this->initPollingTimer();
        }

        /**
         * @brief Release driver resources
         */
        ~Direct() {
            this->deallocPollingTimer();
        }

    private:
        void initPollingTimer();
        void deallocPollingTimer();

        void initGpio();
        void updateButtonState();

        void sendUpdate(const std::unordered_map<Button, bool> &);

    private:
        /// Whether button state changes are logged
        constexpr static const size_t kLogChanges{false};

        /// Time interval for polling timer, in microseconds
        constexpr static const size_t kPollInterval{100'000};

        /// IO expander to whomst we're connected
        std::shared_ptr<drivers::gpio::GpioChip> gpio;
        /// Polling mode timer
        struct event *pollingTimer{nullptr};

        /// Active GPIO bits (these correspond to buttons)
        std::bitset<32> buttonBits;
        /// Polarity of buttons (1 = active high)
        std::bitset<32> buttonPolarity;
        /// Last button state
        std::bitset<32> buttonLastState;

        /// Mapping from IO lines to button
        std::unordered_map<size_t, Button> buttonMap;
};
}

#endif
