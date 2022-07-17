#include <fmt/format.h>
#include <plog/Log.h>

#include "HomeScreen.h"

#include <shittygui/Screen.h>
#include <shittygui/Widgets/Button.h>
#include <shittygui/Widgets/Container.h>
#include <shittygui/Widgets/Label.h>

using namespace Gui;

/**
 * @brief Initialize the home screen
 */
HomeScreen::HomeScreen() {
    // create the container
    auto cont = shittygui::MakeWidget<shittygui::widgets::Container>({0, 0}, {800, 480});
    cont->setDrawsBorder(false);
    cont->setBorderRadius(0.);
    cont->setBackgroundColor({0, 0, 0});

    // actual value section
    auto actualCont = shittygui::MakeWidget<shittygui::widgets::Container>({10, 10}, {340, 350});
    this->initActualValueBox(actualCont);
    cont->addChild(actualCont);

    // system configuration
    auto confCont = shittygui::MakeWidget<shittygui::widgets::Container>({10, 370}, {340, 100});
    this->initConfigBox(confCont);
    cont->addChild(confCont);

    // action buttons
    auto btnCont = shittygui::MakeWidget<shittygui::widgets::Container>({360, 370}, {420, 100});
    this->initActionsBox(btnCont);
    cont->addChild(btnCont);

    // finish set up
    this->root = cont;
}

/**
 * @brief Initialize the actual values container
 *
 * This box is displayed on the left side of the screen.
 */
void HomeScreen::initActualValueBox(const std::shared_ptr<shittygui::widgets::Container> &box) {
    constexpr static const size_t kUnitWidth{69};
    constexpr static const size_t kYSpacing{80};

    // set up the box
    box->setBackgroundColor(kActualBackgroundColor);
    box->setBorderColor(kActualBorderColor);

    // current
    this->actualCurrentLabel = MakeMeasureLabel(box, kActualCurrentColor,
            shittygui::Point(5, (kYSpacing * 0)), "A", kUnitWidth);
    this->actualCurrentLabel->setContent("0.000");

    // voltage
    this->actualVoltageLabel = MakeMeasureLabel(box, kActualVoltageColor,
            shittygui::Point(5, (kYSpacing * 1)), "V", kUnitWidth);
    this->actualVoltageLabel->setContent("0.00");

    // voltage
    this->actualWattageLabel = MakeMeasureLabel(box, kActualWattageColor,
            shittygui::Point(5, (kYSpacing * 2)), "W", kUnitWidth);
    this->actualWattageLabel->setContent("0.00");

    // inside temperature
    this->actualTempLabel = MakeMeasureLabel(box, kActualTempColor, {5, 260}, "°C", kUnitWidth);
    this->actualTempLabel->setContent("0.0");
}

/**
 * @brief Initialize the system config box
 *
 * This is located below the actual value container, it shows the configuration of the system. The
 * data displayed includes the active sense input, trigger mode, and aux output config.
 */
void HomeScreen::initConfigBox(const std::shared_ptr<shittygui::widgets::Container> &box) {
    const auto width = box->getBounds().size.width - 10;

    // set up the box
    box->setBackgroundColor(kActualBackgroundColor);
    box->setBorderColor(kActualBorderColor);

    // Sense configuration
    auto senseLabel = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(5, 5 + ((kConfigFontSize * 1.35) + 5) * 1),
            shittygui::Size(width, kConfigFontSize * 1.35));
    senseLabel->setFont(kConfigFont, kConfigFontSize);
    senseLabel->setTextColor(kConfigTextColor);
    senseLabel->setTextAlign(shittygui::TextAlign::Center);

    senseLabel->setContent("VSense: Internal");

    box->addChild(senseLabel);
    this->vSenseLabel = std::move(senseLabel);

    // Current operating mode
    auto modeLabel = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(5, 5 + ((kConfigFontSize * 1.35) + 5) * 0),
            shittygui::Size(width, kConfigFontSize * 1.35));
    modeLabel->setFont(kConfigFont, kConfigFontSize);
    modeLabel->setTextColor(kConfigTextColor);
    modeLabel->setTextAlign(shittygui::TextAlign::Center);

    modeLabel->setContent("Constant Current");

    box->addChild(modeLabel);
    this->modeLabel = std::move(modeLabel);
}

/**
 * @brief Initialize the action button container box
 *
 * This is a box at the bottom right of the screen with various action buttons to act as shortcuts
 * for commonly used menus.
 */
void HomeScreen::initActionsBox(const std::shared_ptr<shittygui::widgets::Container> &box) {
    // set up the box
    box->setBackgroundColor(kActualBackgroundColor);
    box->setBorderColor(kActualBorderColor);

    // trigger setup btn
    auto modeCfg = shittygui::MakeWidget<shittygui::widgets::Button>({5, 5}, {90, 90},
            shittygui::widgets::Button::Type::Push, "Mode");
    modeCfg->setFont(kActionFont, kActionFontSize);

    box->addChild(modeCfg);

    // trigger configuration
    auto trigSetup = shittygui::MakeWidget<shittygui::widgets::Button>({111, 5}, {90, 90},
            shittygui::widgets::Button::Type::Push, "Trigger Setup");
    trigSetup->setFont(kActionFont, kActionFontSize);

    box->addChild(trigSetup);

    // aux output
    auto auxOutSetup = shittygui::MakeWidget<shittygui::widgets::Button>({217, 5}, {90, 90},
            shittygui::widgets::Button::Type::Push, "Aux Out Config");
    auxOutSetup->setFont(kActionFont, kActionFontSize);

    box->addChild(auxOutSetup);

    // I/O setup
    auto ioSetup = shittygui::MakeWidget<shittygui::widgets::Button>({323, 5}, {90, 90},
            shittygui::widgets::Button::Type::Push, "I/O Config");
    ioSetup->setFont(kActionFont, kActionFontSize);

    box->addChild(ioSetup);
}

/**
 * @brief Create a measurement label
 *
 * This consists of two labels: one for the value (taking up the majority of the space) and then
 * the one for the unit on the right.
 *
 * Labels will fill the entire horizontal width of the container, minus a horizontal padding of
 * the x position in the origin.
 *
 * @param container Widget into which the label is inserted
 * @param color Text color for the label
 * @param origin Top left point of the label
 * @param unitStr Unit string to show
 * @param unitWidth How many pixels of space to reserve for the unit
 *
 * @return Pointer to the value label
 */
std::shared_ptr<shittygui::widgets::Label> HomeScreen::MakeMeasureLabel(
        const std::shared_ptr<shittygui::Widget> &container, const shittygui::Color &color,
        const shittygui::Point origin, const std::string_view &unitStr, const size_t unitWidth) {
    // available width for labels
    const auto width = container->getBounds().size.width - (origin.x * 2);
    // horizontal space between value and unit
    constexpr static const size_t kXSpacing{5};

    // create the value label
    const auto valueWidth = width - unitWidth;

    auto value = shittygui::MakeWidget<shittygui::widgets::Label>(origin,
            shittygui::Size(valueWidth, kActualValueFontSize * 1.2));
    value->setFont(kActualValueFont, kActualValueFontSize);
    value->setTextColor(color);
    value->setTextAlign(shittygui::TextAlign::Right);

    container->addChild(value);

    // create the unit label
    const auto unitYOffset = kActualValueFontSize - kActualUnitFontSize - 2;

    auto unit = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(origin.x + valueWidth + kXSpacing, origin.y + unitYOffset),
            shittygui::Size(unitWidth - kXSpacing, kActualUnitFontSize + 5));
    unit->setFont(kActualUnitFont, kActualUnitFontSize);
    unit->setTextColor(color);
    unit->setTextAlign(shittygui::TextAlign::Center);
    unit->setContent(unitStr);
    unit->setEllipsizeMode(shittygui::EllipsizeMode::None);

    container->addChild(unit);

    return value;
}

