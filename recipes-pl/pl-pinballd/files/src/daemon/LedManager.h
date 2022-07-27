#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <memory>
#include <vector>

/**
 * @brief Interface to front panel LED indicators
 *
 * This provides a driver-agnostic interface to the front panel's LED indicators. Drivers will
 * register themselves with this dude, which in turn will call back out to the driver to set the
 * status of a LED.
 *
 * @seeAlso LedManager::DriverInterface
 */
class LedManager {
    public:
        /// Up to a 3 channel color value
        using Color = std::tuple<double, double, double>;

        /// Supported types of indicators
        enum class Indicator: uint32_t {
            /// RGB status LED
            Status                      = 6,
            /// Dual color trigger indicator
            Trigger                     = 7,
            /// Single color overheat indicator
            Overheat                    = 8,
            /// Single color overcurrent indicator
            Overcurrent                 = 9,
            /// Single color error indicator
            Error                       = 10,

            /// Single color mode button (CC)
            BtnModeCc                   = 1,
            /// Single color mode button (CV)
            BtnModeCv                   = 2,
            /// Single color mode button (CW)
            BtnModeCw                   = 3,
            /// Single color mode button (bonus)
            BtnModeExt                  = 4,
            /// Dual color "Load on" button
            BtnLoadOn                   = 5,
            /// Menu button
            BtnMenu                     = 11,
        };

        /**
         * @brief Validate an indicator value
         *
         * Ensure the specified integer is a valid indicator enum value.
         */
        constexpr static inline bool IsValidIndicatorValue(const uint32_t value) {
            switch(value) {
                case static_cast<uint32_t>(Indicator::Status): [[fallthrough]];
                case static_cast<uint32_t>(Indicator::Trigger): [[fallthrough]];
                case static_cast<uint32_t>(Indicator::Overheat): [[fallthrough]];
                case static_cast<uint32_t>(Indicator::Overcurrent): [[fallthrough]];
                case static_cast<uint32_t>(Indicator::Error): [[fallthrough]];
                case static_cast<uint32_t>(Indicator::BtnModeCc): [[fallthrough]];
                case static_cast<uint32_t>(Indicator::BtnModeCv): [[fallthrough]];
                case static_cast<uint32_t>(Indicator::BtnModeCw): [[fallthrough]];
                case static_cast<uint32_t>(Indicator::BtnModeExt): [[fallthrough]];
                case static_cast<uint32_t>(Indicator::BtnLoadOn): [[fallthrough]];
                case static_cast<uint32_t>(Indicator::BtnMenu):
                    return true;

                default:
                    return false;
            }
        }

        /**
         * @brief Abstract base class for LED drivers
         *
         * This implements various methods relied upon by the LED manager to set the state of a
         * LED indicator.
         */
        class DriverInterface {
            public:
                virtual ~DriverInterface() = default;

                /**
                 * @brief Set the brightness of an indicator
                 *
                 * When applied to a multi-color indicator (such as the status LED) this will set
                 * the brightness of all color components to the same value.
                 *
                 * @return Whether the indicator's state was updated
                 */
                virtual bool setIndicatorBrightness(const Indicator which, const double brightness) = 0;

                /**
                 * @brief Set the color value of an indicator
                 *
                 * If an indicator supports more than one color channel, this call can be used to
                 * set their brightness individually.
                 *
                 * @return Whether the indicator's state was updated
                 */
                virtual bool setIndicatorColor(const Indicator which, const Color &color) = 0;

                /**
                 * @brief Get whether the driver supports global brightness control
                 *
                 * Some LED controllers may implement a global dimming factor that's applied to all
                 * LEDs it drives. This can be used by the user interface to offer an option on
                 * the indicator brightness in addition to display brightness.
                 */
                virtual bool supportsIndicatorGlobalBrightness() const {
                    return false;
                }

                /**
                 * @brief Set the master brightness value
                 *
                 * Set the main brightness value (which is used to modulate the brightness of an
                 * individual indicator) for the controller, if supported.
                 */
                virtual void setIndicatorGlobalBrightness(const double brightness) {
                    // default implementation: do nothing
                }
        };

        void registerDriver(const std::shared_ptr<DriverInterface> &driver);

        void setBrightness(const Indicator which, const double brightness);
        void setColor(const Indicator which, const Color &color);

    public:
        /// LED controller drivers
        std::vector<std::weak_ptr<DriverInterface>> drivers;
};

#endif
