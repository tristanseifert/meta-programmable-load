#ifndef GUI_SETUP_AUXOUT_H
#define GUI_SETUP_AUXOUT_H

#include <memory>
#include <string_view>

#include <shittygui/Types.h>
#include <shittygui/ViewController.h>

namespace shittygui {
class Widget;

namespace widgets {
class Checkbox;
class Container;
}
}

namespace Gui::Setup {
/**
 * @brief Auxiliary analog output configuration
 *
 * Config screen to allow selecting the output mode for the auxiliary analog output. It can be
 * selected from various outputs from a list, as well as a scale factor (to translate the value
 * selected into a voltage) and a customizable update rate.
 */
class AuxOut: public shittygui::ViewController {
    private:
        /// Tag values for the output type selection
        enum OutputTag: uintptr_t {
            Current                     = 0x01 << 0,
            Voltage                     = 0x02 << 0,
            Wattage                     = 0x03 << 0,
            Trigger                     = 0x04 << 0,
        };
        /// Tag values for sample rate selection
        enum SampleRateTag: uintptr_t {
            Low                         = 0x1 << 4,
            Medium                      = 0x2 << 4,
            High                        = 0x3 << 4,
        };

    public:
        AuxOut();
        ~AuxOut() = default;

        /**
         * @brief Get the root widget for the screen
         */
        constexpr inline std::shared_ptr<shittygui::Widget> &getWidget() override {
            return this->root;
        }

        /**
         * @brief Return the view title
         */
        std::string_view getTitle() override {
            return "Aux Output Configuration";
        }

        /**
         * @brief Update the current configuration state
         */
        void viewWillAppear(const bool isAnimated) override {
            ViewController::viewWillAppear(isAnimated);
            this->getRemoteState();
        }

        /**
         * @brief
         */
        void viewDidDisappear() override {
            ViewController::viewDidDisappear();
        }

        /**
         * @brief Allow dismissing the view controller when pressing the menu button
         */
        bool shouldDismissOnMenuPress() override {
            return true;
        }

    private:
        void initEnableSection();
        void initMeasurementSelection();

        void getRemoteState();
        void updateRemoteState();

    private:
        /// Root widget for the screen
        std::shared_ptr<shittygui::Widget> root;

        /// Enable checkbox
        std::shared_ptr<shittygui::widgets::Checkbox> enableCheck;
        /// Container holding the remaining configuration options
        std::shared_ptr<shittygui::widgets::Container> configContainer;
};
}

#endif
