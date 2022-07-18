#ifndef GUI_HOMESCREEN_H
#define GUI_HOMESCREEN_H

#include <string_view>

#include <shittygui/Types.h>

namespace shittygui {
class Widget;

namespace widgets {
class Container;
class ImageView;
class Label;
}
}

namespace Gui {
/**
 * @brief Programmable load home screen
 *
 * This is the main screen for the load. It shows the setpoint value (and allows its adjustment)
 * as well as the current measured voltage, current, and calculated wattage.
 */
class HomeScreen {
    public:
        HomeScreen();
        ~HomeScreen();

        /**
         * @brief Get the root widget for the home screen
         */
        constexpr inline std::shared_ptr<shittygui::Widget> &getWidget() {
            return this->root;
        };

    private:
        void initClockTimer();
        void initActualValueBox(const std::shared_ptr<shittygui::widgets::Container> &);
        void initConfigBox(const std::shared_ptr<shittygui::widgets::Container> &);
        void initActionsBox(const std::shared_ptr<shittygui::widgets::Container> &);
        void initClockBox(const std::shared_ptr<shittygui::widgets::Container> &);

        static std::shared_ptr<shittygui::widgets::Label> MakeMeasureLabel(
                const std::shared_ptr<shittygui::Widget> &container, const shittygui::Color &color,
                const shittygui::Point origin, const std::string_view &unit,
                const size_t unitWidth = 74);

        void updateClock();

    private:
        /// Color for the actual current/voltage region border
        constexpr static const shittygui::Color kActualBorderColor{.4, .4, .4};
        /// Background for the actual current/voltage section
        constexpr static const shittygui::Color kActualBackgroundColor{0, 0, 0};

        /// Font for the actual current/voltage/wattage/etc
        constexpr static const std::string_view kActualValueFont{"DINish Bold"};
        /// Font size for the actual current/voltage/wattage values
        constexpr static const double kActualValueFontSize{65.};
        /// Font for the actual value unit labels
        constexpr static const std::string_view kActualUnitFont{"DINish Condensed"};
        /// Font size for the actual unit labels
        constexpr static const double kActualUnitFontSize{44.};

        /// Font color for the actual current
        constexpr static const shittygui::Color kActualCurrentColor{255./255., 153./255., 200./255.};
        /// Font color for the actual voltage
        constexpr static const shittygui::Color kActualVoltageColor{252./255., 246./255., 189./255.};
        /// Font color for the actual wattage
        constexpr static const shittygui::Color kActualWattageColor{208./255., 244./255., 222./255.};
        /// Font color for the internal temperature
        constexpr static const shittygui::Color kActualTempColor{169./255., 222./255., 249./255.};

        /// Font for the configuration section
        constexpr static const std::string_view kConfigFont{"Liberation Sans"};
        /// Font size for the configuration section
        constexpr static const double kConfigFontSize{20.};
        /// Font color for system config labels
        constexpr static const shittygui::Color kConfigTextColor{1, 1, 1};

        /// Font for action buttons
        constexpr static const std::string_view kActionFont{"Liberation Sans"};
        /// Font size for action buttons
        constexpr static const double kActionFontSize{18.};

        constexpr static const std::string_view kClockFont{"Liberation Sans Narrow"};
        //constexpr static const std::string_view kClockFont{"Dinish Condensed"};
        constexpr static const double kClockFontSize{19.};
        constexpr static const shittygui::Color kClockTextColor{0.94, 0.94, 0.94};

    private:
        /// Root widget for the screen
        std::shared_ptr<shittygui::Widget> root;

        /// Measured input current label
        std::shared_ptr<shittygui::widgets::Label> actualCurrentLabel;
        std::shared_ptr<shittygui::widgets::Label> actualVoltageLabel;
        std::shared_ptr<shittygui::widgets::Label> actualWattageLabel;
        std::shared_ptr<shittygui::widgets::Label> actualTempLabel;

        /// VSense status label
        std::shared_ptr<shittygui::widgets::Label> vSenseLabel;
        /// Current operating mode label
        std::shared_ptr<shittygui::widgets::Label> modeLabel;

        /// Icon for the network connectivity status
        std::shared_ptr<shittygui::widgets::ImageView> statusNet;
        /// Status icon for temperature of instrument
        std::shared_ptr<shittygui::widgets::ImageView> statusTemp;
        /// status icon indicating a remote control connection is present
        std::shared_ptr<shittygui::widgets::ImageView> statusRemote;

        /// clock timer event
        struct event *clockTimerEvent{nullptr};
        /// Clock timer label
        std::shared_ptr<shittygui::widgets::Label> clockLabel;
};
}

#endif
