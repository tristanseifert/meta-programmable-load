#ifndef DRIVERS_LCD_NT35510_H
#define DRIVERS_LCD_NT35510_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <vector>

namespace Drivers::Gpio {
class GpioChip;
}

namespace Drivers::Lcd {
/**
 * @brief NT35510-based TFT LCD driver
 *
 * A basic driver for NT35510-based LCDs, using the host's RGB (parallel) pixel bus. These
 * displays will typically be 800x480 or thereabouts.
 *
 * The following panels based on this chip are supported:
 *
 * - EastRising ER-TFT040-1
 */
class Nt35510 {
    public:
        Nt35510(const std::filesystem::path &spidev,
                const std::shared_ptr<Drivers::Gpio::GpioChip> &gpioChip, const size_t gpioLine);
        ~Nt35510();

    private:
        /**
         * @brief Initialization information for a panel
         *
         * This struct wraps information about a particular panel, including the register config
         * necessary to make it work.
         */
        struct PanelData {
            /// Panel name
            std::string name;
            /// Registers to set during initialization (terminate with reg = 0)
            const std::vector<std::pair<uint16_t, uint8_t>> initRegs;
        };

        void openDevice();
        void closeDevice();

        void runInitSequence(const PanelData &);
        void enableDisplay();

        void toggleReset();

        void setPositionVertical(const uint16_t xs, const uint16_t xe, const uint16_t ys,
                const uint16_t ye) {
            this->regWrite(0x2a00, (xs >> 8));
            this->regWrite(0x2a01, (xs & 0xff));
            this->regWrite(0x2a02, (xe >> 8));
            this->regWrite(0x2a03, (xe & 0xff));
            this->regWrite(0x2b00, (ys >> 8));
            this->regWrite(0x2b01, (ys & 0xff));
            this->regWrite(0x2b02, (ye >> 8));
            this->regWrite(0x2b03, (ye & 0xff));
            this->regWrite(0x2c00);
        }

        void regWrite(const uint16_t address, std::optional<const uint8_t> value = std::nullopt);
        void regRead(const uint16_t address, uint8_t &outValue);
        uint8_t writeWord(const bool rw, const bool dc, const bool upper, const uint8_t payload = 0);

    private:
        /// Total number of supported displays
        constexpr static const size_t kNumPanels{1};
        /// Initialization info for all supported displays
        static const std::array<PanelData, kNumPanels> gPanelData;

        /// File descriptor to the SPI device
        int dev{-1};
        /// Filesystme path to SPI device
        std::filesystem::path devPath;
        /// Chip select line
        struct gpiod_line *devCs{nullptr};

        /// Frequency to use for SPI device accesses (Hz)
        uint32_t busSpeed{5'000'000};

        /// GPIO chip the reset line is connected to
        std::shared_ptr<Drivers::Gpio::GpioChip> gpioChip;
        /// GPIO line the reset signal is connected to
        size_t gpioLine;
};
}

#endif
