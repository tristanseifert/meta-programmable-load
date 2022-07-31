#ifndef DRIVERS_DRIVERLIST_H
#define DRIVERS_DRIVERLIST_H

#include <array>
#include <cstddef>
#include <functional>
#include <string_view>

#include <cbor.h>
#include <uuid.h>

#include "drivers/touch/Ft6336.h"

#include "drivers/button/Direct.h"
#include "drivers/gpio/Pca9535.h"
#include "drivers/lcd/Nt35510.h"
#include "drivers/led/Pca9955.h"

class Probulator;

/// XXX: this is the IO expander used on the rev 3 front panel
static std::shared_ptr<drivers::gpio::Pca9535> gFrontIoExpander;

/**
 * @brief Driver information structure
 *
 * This structure is stored in the driver list and contains information about a driver, including
 * its hardware ID and how to initialize it.
 */
struct DriverInfo {
    /**
     * @brief Unique id for driver (uuid)
     *
     * This is the ID stored in the IDPROM of the hardware; it identifies this driver.
     */
    const uuids::uuid id;

    /**
     * @brief Human-readable driver name
     */
    const std::string_view name;

    /**
     * @brief Construction callback
     *
     * Invoked when the probulator detects a driver initialization request with the driver's
     * unique id.
     */
    std::function<void(Probulator *whomst, const uuids::uuid &driverId,
            const cbor_item_t *payload)> constructor;
};

/**
 * @brief Driver list
 *
 * This contains information on all drivers supported.
 */
static const std::array<DriverInfo, 3> gSupportedDrivers{{
    // FT3663 touch controller
    {
        .id = drivers::touch::Ft6336::kDriverId,
        .name = "FocalTech FT6336 Touch Controller",
        .constructor = [](auto probulator, auto id, auto args) {
            auto driver = std::make_shared<drivers::touch::Ft6336>(probulator->getBusFd(), args);
            probulator->registerDriver(driver);
        }
    },

    /*
     * Direct control over Nt35510 display controller (via SPI)
     *
     * This is only for compatibility with rev3 hardware. On rev4 hardware, the display is
     * controlled through the embedded controller.
     *
     * Additionally, this sets up a small wrapper around the IO expander that polls the state of
     * the buttons connected directly to it. The mapping of inputs is fixed, again for
     * compatibility with rev3 hardware.
     */
    {
        .id = uuids::uuid{{0x08, 0x81, 0xBD, 0xAD, 0x2F, 0xD4, 0x45, 0xB0, 0x84, 0x36, 0x36, 0xDB, 0x75, 0x36, 0xA1, 0x9E}},
        .name = "NT35510 Display Controller",
        .constructor = [](auto probulator, auto id, auto args) {
            if(!gFrontIoExpander) {
                gFrontIoExpander = std::make_shared<drivers::gpio::Pca9535>(probulator->getBusFd(), 0x20);
            }

            drivers::lcd::Nt35510("/dev/spidev0.1", gFrontIoExpander, 8);

            // set up also the direct button io
            auto btn = std::make_shared<drivers::button::Direct>(gFrontIoExpander);
            probulator->registerDriver(btn);
        }
    },

    /**
     * @brief Front panel indicator/button driver (PCA9955B direct control)
     *
     * Implements a driver for the PCA9955B 16-channel LED driver chip. It's connected to various
     * LED indicators and buttons on the front panel.
     */
    {
        .id = drivers::led::Pca9955::kDriverId,
        .name = "PCA9955B 16-channel LED Driver",
        .constructor = [](auto probulator, auto id, auto args) {
            // XXX: only necessary for rev 3 hardware
            // set LED_OE = 0
            gFrontIoExpander->configurePin(7, drivers::gpio::Pca9535::PinMode::Output);
            gFrontIoExpander->setPinState(7, false);

            // create the driver
            auto driver = std::make_shared<drivers::led::Pca9955>(probulator->getBusFd(), args);
            probulator->registerDriver(driver);
        },
    },
}};


#endif
