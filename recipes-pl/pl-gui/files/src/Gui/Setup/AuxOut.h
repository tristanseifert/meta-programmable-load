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

namespace Rpc {
class LoaddClient;
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
    public:
        AuxOut(const std::weak_ptr<Rpc::LoaddClient> &rpc);
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
         * @brief
         */
        void viewWillAppear(const bool isAnimated) override {
            ViewController::viewWillAppear(isAnimated);
        }

        /**
         * @brief
         */
        void viewDidDisappear() override {
            ViewController::viewDidDisappear();
        }

    private:
        void initEnableSection();
        void initMeasurementSelection();

    private:
        /// Reference to the loadd RPC
        std::weak_ptr<Rpc::LoaddClient> loaddRpc;

        /// Root widget for the screen
        std::shared_ptr<shittygui::Widget> root;

        /// Enable checkbox
        std::shared_ptr<shittygui::widgets::Checkbox> enableCheck;
        /// Container holding the remaining configuration options
        std::shared_ptr<shittygui::widgets::Container> configContainer;
};
}

#endif
