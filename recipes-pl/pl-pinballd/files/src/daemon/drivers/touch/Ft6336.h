#ifndef DRIVERS_TOUCH_FT6336_H
#define DRIVERS_TOUCH_FT6336_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include <uuid.h>

#include "drivers/Driver.h"

struct cbor_item_t;

namespace drivers::touch {
/**
 * @brief FocalTech capacitive touch controller driver
 *
 * This implements a driver for the FocalTech FT6336 touch controller; it likely works for other
 * related chips as well.
 */
class Ft6336: public DriverBase, public std::enable_shared_from_this<Ft6336> {
    public:
        Ft6336(const int busFd, const cbor_item_t *config);

    private:
        void readConfig(const cbor_item_t *);

    public:
        /// Hardware driver id
        constexpr static const uuids::uuid kDriverId{{
            0xD5, 0xA6, 0xC9, 0xDF, 0x23, 0xE8, 0x4C, 0x9F, 0xAB, 0xA7, 0x4D, 0x22, 0xF4, 0x3E, 0x51, 0xB1
        }};

    private:
        /// I²C bus address for the controller
        uint8_t address{0};
        /// File descriptor for the I²C bus the controller is on
        int busFd{-1};

        /// Whether interrupts are enabled
        uintptr_t irqEnabled            :1{false};
};

}

#endif
