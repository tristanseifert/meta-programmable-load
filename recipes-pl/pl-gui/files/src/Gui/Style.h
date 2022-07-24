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
#include <shittygui/Widgets/RadioButton.h>

namespace Gui {
namespace Style {
/// Style definitions for checkboxes
struct Checkbox {
    /// Height of a checkbox
    constexpr static const uint16_t kSize{60};
};

/// Style definitions for labels
struct Label {

};

/// Style definitions for radio buttons
struct RadioButton {
    /// Height of a radio button (and thus its radius)
    constexpr static const uint16_t kSize{55};
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
            widget->setFont("Liberation Sans", 23);
            widget->setTextColor({1, 1, 1});
        }

        /**
         * @brief Apply a container default styles
         */
        static inline void Apply(const std::shared_ptr<shittygui::widgets::Container> &widget) {
            widget->setBackgroundColor({0, 0, 0});
            widget->setBorderColor({.4, .4, .4});
        }

        /**
         * @brief Apply a radio button's default styles
         */
        static inline void Apply(const std::shared_ptr<shittygui::widgets::RadioButton> &widget) {
            widget->setFont("Liberation Sans", 23);
            widget->setTextColor({1, 1, 1});
        }
};
}

#endif
