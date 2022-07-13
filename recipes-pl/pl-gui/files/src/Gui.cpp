#include <stdexcept>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_CACHE_H
#include FT_SIZES_H

#include <event2/event.h>
#include <lvgl.h>

#include <fmt/format.h>
#include <plog/Log.h>

#include "EventLoop.h"
#include "Framebuffer.h"
#include "Gui.h"



/**
 * @brief Initialize GUI library
 */
Gui::Gui(const std::shared_ptr<EventLoop> &ev, const std::shared_ptr<Framebuffer> &fb) : ev(ev),
    fb(fb) {
    bool ok;

    // set up lvgl
    lv_init();

    // initialize drivers
    ok = lv_freetype_init(LV_FREETYPE_CACHE_FT_FACES, LV_FREETYPE_CACHE_FT_SIZES,
            LV_FREETYPE_CACHE_SIZE);
    if(!ok) {
        throw std::runtime_error("lv_freetype_init failed");
    }

    // create events
    this->initEvents();
}

/**
 * @brief Create events for lvgl
 *
 * This includes the tick event (used to increment lvgl's internal time) and the timer callback
 */
void Gui::initEvents() {
    auto evbase = this->ev.lock()->getEvBase();

    // create the tick event
    this->tickEvent = event_new(evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        lv_tick_inc(kTickInterval / 1'000U);
    }, this);
    if(!this->tickEvent) {
        throw std::runtime_error("failed to allocate tick event");
    }

    struct timeval tv1{
        .tv_sec  = static_cast<time_t>(kTickInterval / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(kTickInterval % 1'000'000U),
    };

    evtimer_add(this->tickEvent, &tv1);

    // then, the timer event
    this->timerEvent = event_new(evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        lv_timer_handler_run_in_period(kTimerInterval / 1'000U);
    }, this);
    if(!this->timerEvent) {
        throw std::runtime_error("failed to allocate timer event");
    }

    struct timeval tv2{
        .tv_sec  = static_cast<time_t>(kTimerInterval / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(kTimerInterval % 1'000'000U),
    };

    evtimer_add(this->timerEvent, &tv2);
}

/**
 * @brief Tear down GUI library
 */
Gui::~Gui() {
    // remove events
    event_free(this->tickEvent);
    event_free(this->timerEvent);

    // clean up GUI resources
    lv_freetype_destroy();
}
