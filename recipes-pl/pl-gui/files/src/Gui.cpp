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

    this->initDisplay();
    // TODO: set up input methods

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
 * @brief Initialize the display driver
 *
 * This creates a display driver that operates in direct mode, rendering straight to the output
 * framebuffers. The specified flush callback is responsible for copying all redrawn areas from
 * the framebuffer we just requested to output, to the currently displaying one.
 */
void Gui::initDisplay() {
    // initialize the driver struct
    this->dispDriver = new lv_disp_drv_t;
    lv_disp_drv_init(this->dispDriver);

    const auto &size = this->fb->getSize();

    this->dispDriver->hor_res = size.first;
    this->dispDriver->ver_res = size.second;
    this->dispDriver->user_data = this;
    this->dispDriver->flush_cb = [](auto drv, auto area, auto fbPtr) {
        auto gui = reinterpret_cast<Gui *>(drv->user_data);
        const auto fbIdx = gui->fb->indexForFb(fbPtr);

        // notify lvgl we're done
        lv_disp_flush_ready(drv);

        // request flipping of buffer
        gui->fb->requestFbFlip(fbIdx);
    };

    // handle screen rotation
    // TODO: implement

    // set up the buffer handling
    this->dispDriver->direct_mode = true;
    this->dispDriver->full_refresh = true;

    this->dispBuffer = new lv_disp_draw_buf_t;
    lv_disp_draw_buf_init(this->dispBuffer, this->fb->getData(0).data(),
            this->fb->getData(1).data(), (size.first * size.second));

    this->dispDriver->draw_buf = this->dispBuffer;

    // lastly, register display driver
    this->disp = lv_disp_drv_register(this->dispDriver);
    if(!this->disp) {
        throw std::runtime_error("failed to create display");
    }
}

/**
 * @brief Tear down GUI library
 */
Gui::~Gui() {
    // remove events
    event_free(this->tickEvent);
    event_free(this->timerEvent);

    // clean up GUI resources
    lv_disp_remove(this->disp);

    delete this->dispDriver;
    delete this->dispBuffer;

    lv_freetype_destroy();
}
