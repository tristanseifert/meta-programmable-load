#ifndef GUI_H
#define GUI_H

#include <cstddef>
#include <cstdint>
#include <memory>

class EventLoop;
class Framebuffer;

/**
 * @brief LVGL GUI handler
 *
 * This implements a basic display driver (using the Framebuffer class) and event/timer handling
 * via event sources on our main loop.
 */
class Gui {
    public:
        Gui(const std::shared_ptr<EventLoop> &ev, const std::shared_ptr<Framebuffer> &fb);
        ~Gui();

    private:
        void initEvents();

        void initDisplay();

    private:
        /// Tick interval, in microseconds
        constexpr static const size_t kTickInterval{4'000};
        /// Timer interval, in microseconds
        constexpr static const size_t kTimerInterval{5'000};

        /// Main event loop
        std::weak_ptr<EventLoop> ev;
        /// Framebuffer to render to
        std::shared_ptr<Framebuffer> fb;

        /// periodic tick increment event
        struct event *tickEvent{nullptr};
        /// timer callback event
        struct event *timerEvent{nullptr};

        /// Display driver reference
        struct _lv_disp_drv_t *dispDriver{nullptr};
        /// Buffer used by the underlying display driver
        struct _lv_disp_draw_buf_t *dispBuffer{nullptr};
        /// Display instance
        struct _lv_disp_t *disp{nullptr};
};

#endif
