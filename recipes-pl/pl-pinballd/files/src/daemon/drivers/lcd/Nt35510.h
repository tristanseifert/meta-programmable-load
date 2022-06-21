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

        void runInitSequence(const PanelData &);
        void enableDisplay();

        void toggleReset();

        void regWrite(const uint16_t address, std::optional<const uint8_t> value = std::nullopt);
        void regRead(const uint16_t address, uint8_t &outValue);

    private:
        /// Total number of supported displays
        constexpr static const size_t kNumPanels{1};
        /// Initialization info for all supported displays
        static const std::array<PanelData, kNumPanels> gPanelData;

        /// File descriptor to the SPI device
        int dev{-1};

        /// Frequency to use for SPI device accesses (Hz)
        uint32_t busSpeed{1'000'000};

        /// GPIO chip the reset line is connected to
        std::shared_ptr<Drivers::Gpio::GpioChip> gpioChip;
        /// GPIO line the reset signal is connected to
        size_t gpioLine;
};
}

#endif
