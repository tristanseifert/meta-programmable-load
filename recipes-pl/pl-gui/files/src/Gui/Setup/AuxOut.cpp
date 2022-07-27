#include <array>

#include <plog/Log.h>

#include <shittygui/Widgets/Button.h>
#include <shittygui/Widgets/Container.h>
#include <shittygui/Widgets/Checkbox.h>
#include <shittygui/Widgets/Label.h>
#include <shittygui/Widgets/RadioButton.h>

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
            {600, Style::Checkbox::kSize}, true, "Enable Auxiliary Output");
    DefaultStyle::Apply(this->enableCheck);
    this->enableCheck->setPushCallback([this](auto whomst) {
        this->configContainer->setHidden(!this->enableCheck->isChecked());
        this->root->needsDisplay();
    });

    this->root->addChild(this->enableCheck);
}

/**
 * @brief Initialize the output value selection
 */
void AuxOut::initMeasurementSelection() {
    using namespace std::placeholders;

    // label for the type
    auto outputLabel = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(10, 10), shittygui::Size(350, 30),
            "Measurement to Output:");
    outputLabel->setFont("Liberation Sans Medium", 23);
    outputLabel->setTextColor({1, 1, 1}); // TODO: lmao
    outputLabel->setTextAlign(shittygui::TextAlign::Left, shittygui::VerticalAlign::Middle);

    this->configContainer->addChild(outputLabel);

    // value to output
    std::array<shittygui::widgets::RadioButton::GroupEntry, 4> outputOptions{{
        {
            .rect = {{0, 0}, {240, Style::RadioButton::kSize}},
            .label = "Current",
            .tag = OutputTag::Current,
        },
        {
            .rect = {{260, 0}, {240, Style::RadioButton::kSize}},
            .label = "Voltage",
            .tag = OutputTag::Voltage,
        },
        {
            .rect = {{520, 0}, {240, Style::RadioButton::kSize}},
            .label = "Wattage",
            .tag = OutputTag::Wattage,
        },
        {
            .rect = {{0, 65}, {240, Style::RadioButton::kSize}},
            .label = "Trigger",
            .tag = OutputTag::Trigger,
        },
    }};
    auto radioGroup = shittygui::widgets::RadioButton::MakeRadioGroup(outputOptions,
        [](auto &whomst, const auto tag) {
        PLOG_VERBOSE << "Aux out type: " << tag;
    }, [](auto radio) {
        DefaultStyle::Apply(radio);
    });

    radioGroup->setFrameOrigin({10, 52});
    this->configContainer->addChild(radioGroup);

    // label for sample rate
    auto sampleLabel = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(10, 200), shittygui::Size(350, 30),
            "Sample Rate:");
    sampleLabel->setFont("Liberation Sans Medium", 23);
    sampleLabel->setTextColor({1, 1, 1}); // TODO: lmao
    sampleLabel->setTextAlign(shittygui::TextAlign::Left, shittygui::VerticalAlign::Middle);

    this->configContainer->addChild(sampleLabel);

    // sample rate
    std::array<shittygui::widgets::RadioButton::GroupEntry, 3> sampleOptions{{
        {
            .rect = {{0, 0}, {240, Style::RadioButton::kSize}},
            .label = "Low (50Hz)",
            .tag = SampleRateTag::Low,
        },
        {
            .rect = {{260, 0}, {240, Style::RadioButton::kSize}},
            .label = "Med (150Hz)",
            .tag = SampleRateTag::Medium,
        },
        {
            .rect = {{520, 0}, {240, Style::RadioButton::kSize}},
            .label = "High (500Hz)",
            .tag = SampleRateTag::High,
        },
    }};
    auto sampleGroup = shittygui::widgets::RadioButton::MakeRadioGroup(sampleOptions,
        [](auto &whomst, const auto tag) {
        PLOG_VERBOSE << "Sample rate tag: " << tag;
    }, [](auto radio) {
        DefaultStyle::Apply(radio);
    });

    sampleGroup->setFrameOrigin({10, 242});
    this->configContainer->addChild(sampleGroup);
}

/**
 * @brief Fetch from the remote the remote state
 */
void AuxOut::getRemoteState() {
    // TODO: implement
}

/**
 * @brief Send a message to the remote to update its output state
 *
 * This will apply the same configuration as was specified in the UI.
 */
void AuxOut::updateRemoteState() {
    // TODO: yeet
}
