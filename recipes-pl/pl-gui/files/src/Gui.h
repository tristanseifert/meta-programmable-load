#ifndef GUI_H
#define GUI_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

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
        void initDisplayRotated(const size_t angle);

        static void BlitFb(const size_t angle, const std::pair<uint16_t, uint16_t> &size,
                const void *inFb, void *outFb);

    private:
        /// Tick interval, in microseconds
        constexpr static const size_t kTickInterval{4'000};
        /// Timer interval, in microseconds
        constexpr static const size_t kTimerInterval{5'000};

        /// Main event loop
        std::weak_ptr<EventLoop> ev;
        /// Framebuffer to render to
        std::shared_ptr<Framebuffer> fb;

        /**
         * @brief LVGL draw buffer
         *
         * This is a separate buffer so we can implement rotation. It's allocated during driver
         * setup time.
         */
        std::vector<std::byte> drawFramebuffer;
        /// Framebuffer index to render into
        size_t outFbIndex{0};

        /// periodic tick increment event
        struct event *tickEvent{nullptr};
        /// timer callback event
        struct event *timerEvent{nullptr};

        /// Display rotation angle, in degrees
        size_t dispRotation{0};
        /// Display driver reference
        struct _lv_disp_drv_t *dispDriver{nullptr};
        /// Buffer used by the underlying display driver
        struct _lv_disp_draw_buf_t *dispBuffer{nullptr};
        /// Display instance
        struct _lv_disp_t *disp{nullptr};
};

#endif
