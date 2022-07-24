#ifndef GUI_COMMONCONTROLS_H
#define GUI_COMMONCONTROLS_H

#include <shittygui/Widgets/Button.h>
#include <shittygui/Widgets/Container.h>
#include <shittygui/Widgets/Label.h>
#include <shittygui/ViewController.h>

#include "Gui/Style.h"

namespace Gui {
/**
 * @brief Helpers for various generally useful controls
 */
struct CommonControls {
    /**
     * @brief Add a navigation bar at the top of the screen with an option to close the view
     *
     * This will display the title of the view controller centered in the bar.
     *
     * @remark The title will not be updated during the life of the view controller.
     *
     * @param root Root widget of the view controller to add to
     * @param vc View controller instance to reference
     */
    static inline void CreateTopBar(const std::shared_ptr<shittygui::Widget> &root,
            shittygui::ViewController *vc) {
        auto frame = root->getBounds();
        frame.size.height = kTopBarHeight;

        // create a container
        auto container = shittygui::MakeWidget<shittygui::widgets::Container>({0, 0}, frame.size);
        container->setDrawsBorder(false);
        container->setBorderRadius(0.);
        container->setBackgroundColor({0, 0, 0});

        // add the button
        auto closeBtn = shittygui::MakeWidget<shittygui::widgets::Button>({0, 0},
                shittygui::Size(100, frame.size.height - 1),
                shittygui::widgets::Button::Type::Push, "Close");
        DefaultStyle::Apply(closeBtn);
        closeBtn->setBorderWidth(0.);
        closeBtn->setBorderRadius(0.);
        closeBtn->setFont(kTopBarCloseFont, kTopBarCloseFontSize);
        closeBtn->setPushCallback([vc](auto whomst) {
            vc->dismiss(true);
        });

        container->addChild(closeBtn);

        // add the label
        auto label = shittygui::MakeWidget<shittygui::widgets::Label>(
                shittygui::Point(105, 0),
                shittygui::Size(frame.size.width - (105 + 5), frame.size.height - 1));
        label->setFont(kTopBarFont, kTopBarFontSize);
        label->setTextAlign(shittygui::TextAlign::Center, shittygui::VerticalAlign::Middle);
        label->setTextColor(kTopBarTextColor);

        if(vc->getTitle().empty()) {
            label->setContent("(unknown)", false);
        } else {
            label->setContent(vc->getTitle(), false);
        }

        container->addChild(label);

        // add bottom horizontal line
        auto hr = shittygui::MakeWidget<shittygui::widgets::Container>(
                shittygui::Point(0, frame.size.height - 1), shittygui::Size(frame.size.width, 1));
        hr->setDrawsBorder(false);
        hr->setBorderRadius(0.);
        hr->setBackgroundColor({.33, .33, .33});

        container->addChild(hr);

        // done
        root->addChild(container);
    }

    private:
        /// Height of the top nav bar
        constexpr static const size_t kTopBarHeight{64};
        /// Text color for the top bar title
        constexpr static const shittygui::Color kTopBarTextColor{1, 1, 1};
        /// Font for the top nav bar
        constexpr static const std::string_view kTopBarFont{"DINish Expanded Semibold"};
        /// Font size of the top nav bar
        constexpr static const size_t kTopBarFontSize{35};
        /// Font for the top close button
        constexpr static const std::string_view kTopBarCloseFont{"DINish Condensed"};
        /// Font size for the top close button
        constexpr static const size_t kTopBarCloseFontSize{24};
};
}

#endif
