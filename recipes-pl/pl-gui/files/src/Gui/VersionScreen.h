#ifndef GUI_VERSIONSCREEN_H
#define GUI_VERSIONSCREEN_H

#include <memory>
#include <string_view>

#include <shittygui/Types.h>
#include <shittygui/ViewController.h>

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
 * @brief Version Screen
 *
 * Displays the current version of the system and performs a test of the indicators.
 */
class VersionScreen: public shittygui::ViewController {
    public:
        VersionScreen();
        ~VersionScreen();

        /**
         * @brief Get the root widget for the home screen
         */
        constexpr inline std::shared_ptr<shittygui::Widget> &getWidget() override {
            return this->root;
        }

        /**
         * @brief Return the view title
         */
        std::string_view getTitle() override {
            return "Version Information";
        }

        /**
         * @brief Install the periodic timer callback
         */
        void viewWillAppear(const bool isAnimated) override {
            ViewController::viewWillAppear(isAnimated);
            this->initTimer();
            this->runLedTest(0);
        }
        /**
         * @brief Remove the timer callback
         */
        void viewWillDisappear(const bool isAnimated) override {
            ViewController::viewWillDisappear(isAnimated);
            this->removeTimer();
        }

    private:
        void initTimer();
        void removeTimer();
        void timerCallback();
        bool runLedTest(const size_t step);

    private:
        /// Heading label font
        constexpr static const std::string_view kTitleFont{"DINish Condensed Bold"};
        /// Heading label font size
        constexpr static const double kTitleFontSize{64.};

        /// Heading label font
        constexpr static const std::string_view kVersionFont{"Liberation Sans"};
        /// Heading label font size
        constexpr static const double kVersionFontSize{24.};

    private:
        /// Timer event
        struct event *timerEvent{nullptr};
        /// Number of times the timer has fired
        size_t timerCount{0};

        /// Root widget for the screen
        std::shared_ptr<shittygui::Widget> root;
};
}

#endif
