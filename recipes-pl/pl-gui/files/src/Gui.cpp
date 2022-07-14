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

    this->initDisplayRotated(270);
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
 * @brief Initialize the display driver (with rotation support)
 *
 * The display driver renders to an intermediate buffer, which is rotated and copied when drawing
 * has completed.
 *
 * @param rotation Rotation angle (degrees; only 90, 180 and 270° are supported)
 */
void Gui::initDisplayRotated(const size_t angle) {
    // validate inputs
    if(angle != 90 && angle != 180 && angle != 270) {
        throw std::invalid_argument(fmt::format("invalid angle ({})", angle));
    }
    this->dispRotation = angle;

    // initialize the driver struct
    this->dispDriver = new lv_disp_drv_t;
    lv_disp_drv_init(this->dispDriver);

    // allocate a buffer, and zero it such that it's all faulted in
    const auto &size = this->fb->getSize();
    const auto numPixels = static_cast<size_t>(size.first) * static_cast<size_t>(size.second);

    this->drawFramebuffer.resize(numPixels * 4, std::byte{0});

    // set dimensions according to rotation
    if(angle == 90 || angle == 270) {
        this->dispDriver->hor_res = size.second;
        this->dispDriver->ver_res = size.first;
    } else {
        this->dispDriver->hor_res = size.first;
        this->dispDriver->ver_res = size.second;
    }

    // set up flush callback (it will rotate)
    this->dispDriver->user_data = this;
    this->dispDriver->flush_cb = [](auto drv, auto area, auto fbPtr) {
        auto gui = reinterpret_cast<Gui *>(drv->user_data);
        const auto &size = gui->fb->getSize();
        auto dispFb = gui->fb->getData(gui->outFbIndex);

        BlitFb(gui->dispRotation, size, fbPtr, dispFb.data());

        // notify lvgl we're done
        lv_disp_flush_ready(drv);
    };

    // set up the buffer handling
    this->dispDriver->direct_mode = true;

    this->dispBuffer = new lv_disp_draw_buf_t;
    lv_disp_draw_buf_init(this->dispBuffer, this->drawFramebuffer.data(), nullptr, numPixels);

    this->dispDriver->draw_buf = this->dispBuffer;

    // lastly, register display driver
    this->disp = lv_disp_drv_register(this->dispDriver);
    if(!this->disp) {
        throw std::runtime_error("failed to create display");
    }

    /**
     * synchronize the GUI drawing to vblank
     */
    lv_timer_del(this->disp->refr_timer);
    this->disp->refr_timer = nullptr;

    this->fb->addSwapCallback([&](auto bufIdx) {
        this->outFbIndex = bufIdx;
        _lv_disp_refr_timer(nullptr);
    });
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



/**
 * @brief Blit a framebuffer with optional rotation
 *
 * @TODO optimize using NEON
 *
 * @param angle Rotation angle (must be a multiple of 90°, up to 270°)
 * @param size Pixel size (width, height) of the output buffer
 * @param inFb Pointer to the start of the input (possibly rotated) framebuffer
 * @param outFb Pointer to the display output buffer
 */
void Gui::BlitFb(const size_t angle, const std::pair<uint16_t, uint16_t> &size,
        const void *inFb, void *outFb) {
    auto inPtr = reinterpret_cast<const uint32_t *>(inFb);
    auto outPtr = reinterpret_cast<uint32_t *>(outFb);

    if(angle == 270) {
        for(size_t y = 0; y < size.second; y++) {
            for(size_t x = 0; x < size.first; x++) {
                //outPtr[(y * size.first) + (size.first - x - 1)] = inPtr[(x * size.second) + (y)];
                //outPtr[(y * size.first) + x] = inPtr[(x * size.second) + (y)];
                outPtr[((size.second - 1 - y) * size.first) + x] = inPtr[(x * size.second) + (y)];
            }
        }
    }
    // no rotation (or unsupported rotation)
    else {
        memcpy(outPtr, inPtr,
                static_cast<size_t>(size.first) * static_cast<size_t>(size.second) * 4);
    }

}

