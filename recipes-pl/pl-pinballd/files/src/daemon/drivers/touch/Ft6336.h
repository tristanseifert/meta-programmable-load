#ifndef DRIVERS_TOUCH_FT6336_H
#define DRIVERS_TOUCH_FT6336_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

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
    private:
        /**
         * @brief Device register addresses
         */
        enum class Register: uint8_t {
            /**
             * @brief Touch down status
             *
             * Indicates the number of detected touch points. Values of 0-2 are valid.
             */
            TouchStatus                 = 0x02,

            Point1XHigh                 = 0x03,

            LibVersionH                 = 0xA1,
            LibVersionL                 = 0xA2,
            FirmwareVersion             = 0xA6,
            ManufacturerID              = 0xA8,
            ReleaseCode                 = 0xAF,
        };

    public:
        using TouchPosition = std::pair<uint16_t, uint16_t>;

        Ft6336(const int busFd, const cbor_item_t *config);
        ~Ft6336();

    private:
        void readConfig(const cbor_item_t *);
        void initPollingTimer();

        void updateTouchState();
        void clearTouchPoint(const size_t point);
        void decodeTouchPoint(const size_t point, std::span<const uint8_t, 6> regData);

        void sendTouchStateUpdate();
        struct cbor_item_t *encodeTouchState(const TouchPosition &);

        /**
         * @brief Read a device register
         *
         * @param reg Device register address
         *
         * @return Read register value
         *
         * @throws std::system_error IO error
         */
        inline uint8_t readRegister(const Register reg) {
            std::array<uint8_t, 1> buffer;
            this->readRegisters(reg, buffer, 1);
            return buffer[0];
        }
        void readRegisters(const Register start, std::span<uint8_t> outBuffer,
                const size_t numRegs = 0);
        void writeRegister(const Register reg, const uint8_t value);

        /**
         * @brief Transform the coordinate of a touch point based on rotation
         */
        inline void transformTouchPosition(TouchPosition &pos) {
            switch(this->rotation) {
                // no rotation
                case 0:
                    break;
                // 90°
                case 1: // XXX: this is untested
                    std::swap(pos.first, pos.second);
                    break;
                // 180°
                case 2: // XXX: this is untested
                    pos.second = this->size.second - pos.second;
                    break;
                // 270°
                case 3:
                    pos.second = this->size.second - pos.second;
                    std::swap(pos.first, pos.second);
                    break;
            }
        }

    public:
        /// Hardware driver id
        constexpr static const uuids::uuid kDriverId{{
            0xD5, 0xA6, 0xC9, 0xDF, 0x23, 0xE8, 0x4C, 0x9F, 0xAB, 0xA7, 0x4D, 0x22, 0xF4, 0x3E, 0x51, 0xB1
        }};

    private:
        /// Time interval for polling timer, in microseconds
        constexpr static const size_t kPollInterval{33'333};

        /// Device firmware version
        uint8_t firmwareVersion;

        /// I²C bus address for the controller
        uint8_t address{0};
        /// File descriptor for the I²C bus the controller is on
        int busFd{-1};

        /// Physical size of the display panel (in points)
        std::pair<uint16_t, uint16_t> size{0, 0};
        /// Current positions of touch points
        std::array<TouchPosition, 2> touchPositions;
        /// ID of the touch event in the slot
        std::array<uint8_t, 2> touchIds;

        /// Polling mode timer
        struct event *pollingTimer{nullptr};

        /// Whether interrupts are enabled
        uintptr_t irqEnabled            :1{false};
        /// Does touch point 1 have valid data?
        uintptr_t p1HasData             :1{false};
        /// Does touch point 2 have valid data?
        uintptr_t p2HasData             :1{false};
        /**
         * @brief Coordinate rotation flag
         *
         * This contains the rotation of the touch panel relative to how it reports touch events
         * in units of 90°.
         */
        uintptr_t rotation              :2{0};
};

}

#endif
