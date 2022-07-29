#include <array>
#include <stdexcept>
#include <vector>

#include <event2/event.h>
#include <fmt/format.h>
#include <plog/Log.h>
#include <load-common/EventLoop.h>

#include "VersionScreen.h"
#include "HomeScreen.h"

#include "Gui/IconManager.h"
#include "Gui/Style.h"
#include "Rpc/PinballClient.h"
#include "version.h"

#include <shittygui/Screen.h>
#include <shittygui/Widgets/Container.h>
#include <shittygui/Widgets/Label.h>

using namespace Gui;

/**
 * @brief Initialize the home screen
 */
VersionScreen::VersionScreen(const std::shared_ptr<Rpc::LoaddClient> &loaddRpc) : loaddRpc(loaddRpc) {
    // create the container
    auto cont = shittygui::MakeWidget<shittygui::widgets::Container>({0, 0}, {800, 480});
    cont->setDrawsBorder(false);
    cont->setBorderRadius(0.);
    cont->setBackgroundColor({0, 0, 0});

    // heading
    auto titleLabel = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(5, (480 / 2) - ((kTitleFontSize * 1.35) / 2)),
            shittygui::Size(790, kTitleFontSize * 1.35));
    titleLabel->setFont(kTitleFont, kTitleFontSize);
    titleLabel->setTextColor({1, 1, 1});
    titleLabel->setTextAlign(shittygui::TextAlign::Center);
    titleLabel->setContent("Programmable Load");
    cont->addChild(titleLabel);

    // version label
    auto versionLabel = shittygui::MakeWidget<shittygui::widgets::Label>(
            shittygui::Point(5, 440),
            shittygui::Size(790, kVersionFontSize * 1.35));
    versionLabel->setFont(kVersionFont, kVersionFontSize);
    versionLabel->setTextColor({1, 1, 1});
    versionLabel->setTextAlign(shittygui::TextAlign::Center);
    versionLabel->setContent(fmt::format("Version {} ({})", kVersion, kVersionGitHash));
    cont->addChild(versionLabel);

    this->root = cont;
}

/**
 * @brief Clean up the version screen resources
 */
VersionScreen::~VersionScreen() {
    this->removeTimer();
}

/**
 * @brief Initialize the periodic timer
 *
 * This timer fires roughly every 500ms, and is used to process the indicator test. It will also
 * be used to dismiss the view.
 */
void VersionScreen::initTimer() {
    auto evbase = PlCommon::EventLoop::Current()->getEvBase();
    this->timerEvent = event_new(evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        reinterpret_cast<VersionScreen *>(ctx)->timerCallback();
    }, this);
    if(!this->timerEvent) {
        throw std::runtime_error("failed to allocate timer event");
    }

    struct timeval tv{
        .tv_sec  = static_cast<time_t>(0),
        .tv_usec = static_cast<suseconds_t>(500'000),
    };

    this->timerCount = 0;
    evtimer_add(this->timerEvent, &tv);
}

/**
 * @brief Remove the periodic timer
 */
void VersionScreen::removeTimer() {
    if(this->timerEvent) {
        event_free(this->timerEvent);
        this->timerEvent = nullptr;
    }
}

/**
 * @brief Timer periodic callback
 *
 * Drive the indicator test state machine, and dismiss the screen after the indicator test is
 * complete.
 */
void VersionScreen::timerCallback() {
    const auto done = this->runLedTest(++this->timerCount);
    if(done) {
        auto screen = this->root->getScreen();

        auto home = std::make_shared<Gui::HomeScreen>();
        screen->setRootViewController(home);

        this->removeTimer();
    }
}

/**
 * @brief Update the state of the indicators according to the current step
 *
 * @return Whether we've reached the last entry in the LED test sequence
 */
bool VersionScreen::runLedTest(const size_t step) {
    using Indicator = Rpc::PinballClient::Indicator;
    using Color = Rpc::PinballClient::IndicatorColor;

    static const std::array<const std::vector<const Rpc::PinballClient::IndicatorChange>, 5> gIndicatorStates{{
        // stage 0: test buttons
        {
            // turn off all indicators
            { Indicator::Trigger, false },
            { Indicator::Overheat, false },
            { Indicator::Overcurrent, false },
            { Indicator::Error, false },

            // illuminate the buttons
            { Indicator::BtnModeCc, true },
            { Indicator::BtnModeCv, true },
            { Indicator::BtnModeCw, true },
            { Indicator::BtnModeExt, true },
            { Indicator::BtnLoadOn, Color{1., 0., 0.} },
            { Indicator::BtnMenu, true },

            // status indicator
            { Indicator::Status, Color{1., 0., 0.} },
        },
        // stage 1: indicators
        {
            { Indicator::BtnModeCc, false },
            { Indicator::BtnModeCv, false },
            { Indicator::BtnModeCw, false },
            { Indicator::BtnModeExt, false },
            { Indicator::BtnLoadOn, Color{0., 1., 0.} },
            { Indicator::BtnMenu, false },

            { Indicator::Status, Color{0., 1., 0.} },
            { Indicator::Trigger, Color{1., 0., 0.} },
            { Indicator::Overheat, true },
            { Indicator::Overcurrent, true },
            { Indicator::Error, true },
        },
        // stage 2: more indicators
        {
            { Indicator::BtnLoadOn, false },

            { Indicator::Status, Color{0., 0., 1.} },
            { Indicator::Trigger, Color{0., 1., 0.} },
            { Indicator::Overheat, false },
            { Indicator::Overcurrent, false },
            { Indicator::Error, false },
        },
        // stage 3: even more indicators
        {
            { Indicator::Status, false },
            { Indicator::Trigger, false },
        },
        // stage 4: lmao
        {
        },
    }};

    // set the appropriate indicator state
    // SharedState::gRpcPinball->setIndicatorState(gIndicatorStates.at(step));

    return step == (gIndicatorStates.size() - 1);
}
