/**
 * @file
 *
 * @brief GUI style definitions
 */
#ifndef GUI_STYLE_H
#define GUI_STYLE_H

#include <shittygui/Types.h>
#include <shittygui/Widgets/Button.h>
#include <shittygui/Widgets/Checkbox.h>
#include <shittygui/Widgets/Container.h>

namespace Gui {
namespace Style {
struct Checkbox {
    /// Its size
    constexpr static const shittygui::Size kSize{60, 60};
};
};

/**
 * @brief GUI default style
 *
 * Conglomeration of styles for controls, as well as helpers to apply said style to the controls.
 */
struct DefaultStyle {
    public:
        /**
         * @brief Apply a button's default styles
         */
        static inline void Apply(const std::shared_ptr<shittygui::widgets::Button> &widget) {

        }

        /**
         * @brief Apply a checkbox's default styles
         */
        static inline void Apply(const std::shared_ptr<shittygui::widgets::Checkbox> &widget) {

        }

        /**
         * @brief Apply a container default styles
         */
        static inline void Apply(const std::shared_ptr<shittygui::widgets::Container> &widget) {
            widget->setBackgroundColor({0, 0, 0});
            widget->setBorderColor({.4, .4, .4});
        }
};
}

#endif
