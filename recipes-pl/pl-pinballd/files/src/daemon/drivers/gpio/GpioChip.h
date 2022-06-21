#ifndef DRIVERS_GPIO_GPIOCHIP_H
#define DRIVERS_GPIO_GPIOCHIP_H

#include <cstddef>
#include <cstdint>

namespace Drivers::Gpio {
/**
 * @brief Base class for GPIO sources
 *
 * This class serves as an abstract base interface for all sources of IOs, whether those are on an
 * IO expander, or internal to the SoC.
 */
class GpioChip {
    public:
        enum PinMode: uint32_t {
            Input                       = (0x0 << 0),
            Output                      = (0x1 << 0),
            OutputPushPull              = Output,
            OutputOpenDrain             = (0x2 << 0),

            PullUp                      = (0x1 << 4),
            PullDown                    = (0x2 << 4),

            Inverted                    = (1 << 8),
        };

    public:
        virtual ~GpioChip() = default;

        /**
         * @brief Configure a pin
         *
         * Applies the provided configuration to the pin specified by the given index.
         *
         * @param pin Pin index
         * @param mode A bitwise OR of the PinMode fields
         */
        virtual void configurePin(const size_t pin, const PinMode mode) = 0;

        /**
         * @brief Set the state of an output pin
         */
        virtual void setPinState(const size_t pin, bool asserted) = 0;

        /**
         * @brief Get the state of a pin
         */
        virtual bool getPinState(const size_t pin) {
            return !!(getPinState() & (1 << pin));
        };

        /**
         * @brief Get the s tate of all pins
         *
         * If the pin is an input, this will reflect the current value at the pin; if it's an
         * output, it will reflect the value we're driving to the pin, which may not necessarily
         * be the actual physical value of the pin.
         */
        virtual uint32_t getPinState() = 0;
};
};

#endif
