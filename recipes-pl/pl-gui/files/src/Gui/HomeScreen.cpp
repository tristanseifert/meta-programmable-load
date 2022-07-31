#include <ctime>
#include <iomanip>
#include <sstream>

#include <event2/event.h>

#include <fmt/format.h>
#include <plog/Log.h>
#include <load-common/EventLoop.h>

#include "HomeScreen.h"

#include "Gui/Setup/AuxOut.h"
#include "Gui/IconManager.h"
#include "Gui/Style.h"
#include "Rpc/LoaddClient.h"
#include "SharedState.h"

#include <shittygui/Screen.h>
#include <shittygui/Widgets/Button.h>
#include <shittygui/Widgets/Container.h>
#include <shittygui/Widgets/ImageView.h>
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

    // actual value section (top left)
    auto actualCont = shittygui::MakeWidget<shittygui::widgets::Container>({10, 10}, {340, 370});
    this->initActualValueBox(actualCont);
    cont->addChild(actualCont);

    // system configuration (under actual values)
    auto confCont = shittygui::MakeWidget<shittygui::widgets::Container>({10, 385}, {340, 80});
    this->initConfigBox(confCont);
    cont->addChild(confCont);

    // action buttons (along right side)
    auto btnCont = shittygui::MakeWidget<shittygui::widgets::Container>({690, 10}, {100, 350});
    this->initActionsBox(btnCont);
    cont->addChild(btnCont);

    // current date/time (bottom right)
    auto clockCont = shittygui::MakeWidget<shittygui::widgets::Container>({690, 370}, {100, 100});
    this->initClockBox(clockCont);
    cont->addChild(clockCont);

    // finish set up
    this->root = cont;

    this->initClockTimer();
}

/**
 * @brief Initialize an event to drive the clock
 */
void HomeScreen::initClockTimer() {
    auto evbase = PlCommon::EventLoop::Current()->getEvBase();
    this->clockTimerEvent = event_new(evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        reinterpret_cast<HomeScreen *>(ctx)->updateClock();
    }, this);
    if(!this->clockTimerEvent) {
        throw std::runtime_error("failed to allocate clock event");
    }

    struct timeval tv{
        .tv_sec  = static_cast<time_t>(1),
        .tv_usec = static_cast<suseconds_t>(0),
    };

    evtimer_add(this->clockTimerEvent, &tv);
}

/**
 * @brief Initialize the actual values container
 *
 * This box is displayed on the left side of the screen.
 */
void HomeScreen::initActualValueBox(const std::shared_ptr<shittygui::widgets::Container> &box) {
    constexpr static const size_t kUnitWidth{69};
    constexpr static const size_t kYSpacing = kActualValueHeight;

    // set up the box
    box->setBackgroundColor(kActualBackgroundColor);
    box->setBorderColor(kActualBorderColor);

    // current
    this->actualCurrentLabel = MakeMeasureLabel(box, kActualCurrentColor,
            shittygui::Point(5, (kYSpacing * 0) + 2), "A", kUnitWidth);
    this->actualCurrentLabel->setBackgroundColor({0, 0, 0, 1});
    this->actualCurrentLabel->setContent("<span font_features='tnum'>0.000</span>", true);

    // voltage
    this->actualVoltageLabel = MakeMeasureLabel(box, kActualVoltageColor,
            shittygui::Point(5, (kYSpacing * 1) + 2), "V", kUnitWidth);
    this->actualVoltageLabel->setBackgroundColor({0, 0, 0, 1});
    this->actualVoltageLabel->setContent("<span font_features='tnum'>0.00</span>", true);

    // voltage
    this->actualWattageLabel = MakeMeasureLabel(box, kActualWattageColor,
            shittygui::Point(5, (kYSpacing * 2) + 2), "W", kUnitWidth);
    this->actualWattageLabel->setBackgroundColor({0, 0, 0, 1});
    this->actualWattageLabel->setContent("<span font_features='tnum'>0.00</span>", true);

    // inside temperature
    this->actualTempLabel = MakeMeasureLabel(box, kActualTempColor,
            shittygui::Point(5, (kYSpacing * 3) + 2), "°C", kUnitWidth);
    this->actualTempLabel->setBackgroundColor({0, 0, 0, 1});
    this->actualTempLabel->setContent("<span font_features='tnum'>0.0</span>", true);
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
    auto trigSetup = shittygui::MakeWidget<shittygui::widgets::Button>({5, 105}, {90, 90},
            shittygui::widgets::Button::Type::Push, "Trigger Setup");
    trigSetup->setFont(kActionFont, kActionFontSize);

    box->addChild(trigSetup);

    // aux output
    auto auxOutSetup = shittygui::MakeWidget<shittygui::widgets::Button>({5, 205}, {90, 90},
            shittygui::widgets::Button::Type::Push, "Aux Out Config");
    auxOutSetup->setFont(kActionFont, kActionFontSize);
    auxOutSetup->setPushCallback([this](auto whomst) {
        auto auxCfg = std::make_shared<Setup::AuxOut>();
        this->presentViewController(auxCfg, true);
    });

    box->addChild(auxOutSetup);

    // network icon
    this->statusNet = shittygui::MakeWidget<shittygui::widgets::ImageView>({2, 307}, {32, 32});
    this->statusNet->setBorderWidth(0.);
    this->statusNet->setBackgroundColor({0, 0, 0, 0});
    this->statusNet->setImage(IconManager::LoadIcon(IconManager::Icon::NetworkUp,
                IconManager::Size::Square32));

    box->addChild(this->statusNet);

    // temperature icon
    this->statusTemp = shittygui::MakeWidget<shittygui::widgets::ImageView>({34, 307}, {32, 32});
    this->statusTemp->setBorderWidth(0.);
    this->statusTemp->setBackgroundColor({0, 0, 0, 0});
    this->statusTemp->setImage(IconManager::LoadIcon(IconManager::Icon::TemperatureLowest,
                IconManager::Size::Square32));

    box->addChild(this->statusTemp);

    // remote control icon
    this->statusRemote = shittygui::MakeWidget<shittygui::widgets::ImageView>({66, 307}, {32, 32});
    this->statusRemote->setBorderWidth(0.);
    this->statusRemote->setBackgroundColor({0, 0, 0, 0});
    this->statusRemote->setImage(IconManager::LoadIcon(IconManager::Icon::Disconnected,
                IconManager::Size::Square32));

    box->addChild(this->statusRemote);
}

/**
 * @brief Initialize the clock box
 *
 * This is a label that shows the current date and time.
 */
void HomeScreen::initClockBox(const std::shared_ptr<shittygui::widgets::Container> &box) {
    const auto width = box->getBounds().size.width - 2,
          height = box->getBounds().size.height - 2;

    // set up the box
    box->setBackgroundColor(kActualBackgroundColor);
    box->setBorderColor(kActualBorderColor);

    // create the label
    auto clockLabel = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(1, 2), shittygui::Size(width, height));
    clockLabel->setFont(kClockFont, kClockFontSize);
    clockLabel->setTextColor(kClockTextColor);
    clockLabel->setTextAlign(shittygui::TextAlign::Center,
            shittygui::VerticalAlign::Middle);

    box->addChild(clockLabel);

    this->clockLabel = std::move(clockLabel);
    this->updateClock();
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
            shittygui::Size(valueWidth, kActualValueHeight));
    value->setFont(kActualValueFont, kActualValueFontSize);
    value->setTextColor(color);
    value->setTextAlign(shittygui::TextAlign::Right, shittygui::VerticalAlign::Top);

    container->addChild(value);

    // create the unit label
    constexpr static const int kUnitYOffset{32};

    auto unit = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(origin.x + valueWidth + kXSpacing, origin.y + kUnitYOffset),
            shittygui::Size(unitWidth - kXSpacing, kActualValueHeight - kUnitYOffset));
    unit->setFont(kActualUnitFont, kActualUnitFontSize);
    unit->setTextColor(color);
    unit->setTextAlign(shittygui::TextAlign::Center, shittygui::VerticalAlign::Bottom);
    unit->setContent(unitStr);
    unit->setEllipsizeMode(shittygui::EllipsizeMode::None);

    container->addChild(unit);

    return value;
}

/**
 * @brief Clean up home screen resources
 *
 * Remove the clock timer.1G
 */
HomeScreen::~HomeScreen() {
    if(this->clockTimerEvent) {
        event_free(this->clockTimerEvent);
    }

    this->removeMeasurementCallback();
}



/**
 * @brief Update the clock
 */
void HomeScreen::updateClock() {
    // get current time
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);

    // format time and date string and set it
    std::stringstream str;
    if((tm.tm_sec % 2)) {
        str << std::put_time(&tm, "<span font_features='tnum'>%H %M %S</span>\n%b %d");
    } else {
        str << std::put_time(&tm, "<span font_features='tnum'>%H:%M:%S</span>\n%b %d");
    }

    this->clockLabel->setContent(str.str(), true);
}



/**
 * @brief Install the measurement callback
 *
 * This updates our measurement labels and the thermal state icons.
 */
void HomeScreen::installMeasurementCallback() {
    if(this->measurementCallbackToken) {
        this->removeMeasurementCallback();
    }

    this->measurementCallbackToken = SharedState::gRpcLoadd->addMeasurementCallback([&](const auto &data) {
        this->actualCurrentLabel->setContent(fmt::format("<span font_features='tnum'>{:.3f}</span>", data.current), true);
        this->actualVoltageLabel->setContent(fmt::format("<span font_features='tnum'>{:.2f}</span>", data.voltage), true);
        this->actualTempLabel->setContent(fmt::format("<span font_features='tnum'>{:.1f}</span>", data.temperature), true);
    });
}

/**
 * @brief Remove an existing measurement callback
 */
void HomeScreen::removeMeasurementCallback() {
    if(!this->measurementCallbackToken) {
        return;
    }

    SharedState::gRpcLoadd->removeMeasurementCallback(this->measurementCallbackToken);
    this->measurementCallbackToken = 0;
}
