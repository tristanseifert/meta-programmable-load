#include <plog/Log.h>

#include <shittygui/Widgets/Button.h>
#include <shittygui/Widgets/Container.h>
#include <shittygui/Widgets/Checkbox.h>
#include <shittygui/Widgets/Label.h>

#include "Gui/CommonControls.h"
#include "Gui/Style.h"
#include "Gui/Setup/AuxOut.h"

using namespace Gui::Setup;

/**
 * @brief Initialize the auxiliary output configuration
 */
AuxOut::AuxOut(const std::weak_ptr<Rpc::LoaddClient> &rpc) : loaddRpc(rpc) {
    // create the container
    auto cont = shittygui::MakeWidget<shittygui::widgets::Container>({0, 0}, {800, 480});
    cont->setDrawsBorder(false);
    cont->setBorderRadius(0.);
    cont->setBackgroundColor({0, 0, 0});
    this->root = cont;

    // add the top bar
    CommonControls::CreateTopBar(cont, this);

    // build the enable section
    this->initEnableSection();

    // build content section
    this->configContainer = shittygui::MakeWidget<shittygui::widgets::Container>({10, 150},
            {780, 320});
    DefaultStyle::Apply(this->configContainer);

    this->initMeasurementSelection();

    this->root->addChild(this->configContainer);

    // done setting up
}

/**
 * @brief Initialize the top of the screen with the enable checkbox
 */
void AuxOut::initEnableSection() {
    // create checkbox
    this->enableCheck = shittygui::MakeWidget<shittygui::widgets::Checkbox>({20, 80},
            Style::Checkbox::kSize, true);
    DefaultStyle::Apply(this->enableCheck);
    this->enableCheck->setPushCallback([this](auto whomst) {
        this->configContainer->setHidden(!this->enableCheck->isChecked());
        this->root->needsDisplay();
    });

    this->root->addChild(this->enableCheck);

    // create the adjacent label
    auto checkLabel = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(90, 80), shittygui::Size(400, Style::Checkbox::kSize.height),
            "Enable Aux Analog Output");
    checkLabel->setFont("Liberation Sans", 23);
    checkLabel->setTextColor({1, 1, 1}); // TODO: lmao
    checkLabel->setTextAlign(shittygui::TextAlign::Left, shittygui::VerticalAlign::Middle);

    this->root->addChild(checkLabel);
}

/**
 * @brief Initialize the output value selection
 */
void AuxOut::initMeasurementSelection() {
    // TODO: placeholder
    auto checkLabel = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(10, 160-20), shittygui::Size(780, 40),
            "Placeholder area for content");
    checkLabel->setFont("Liberation Sans", 24);
    checkLabel->setTextColor({1, 1, 1}); // TODO: lmao
    checkLabel->setTextAlign(shittygui::TextAlign::Center, shittygui::VerticalAlign::Middle);

    this->configContainer->addChild(checkLabel);
}
