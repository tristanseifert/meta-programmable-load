#ifndef GUI_RENDERER_H
#define GUI_RENDERER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

class EventLoop;
class Framebuffer;

namespace shittygui {
class Screen;
class ViewController;
}

namespace Gui {
class FontHandler;

/**
 * @brief GUI handler
 *
 * This implements a basic display driver (using the Framebuffer class) and event/timer handling
 * via event sources on our main loop.
 */
class Renderer {
    public:
        Renderer(const std::shared_ptr<EventLoop> &ev, const std::shared_ptr<Framebuffer> &fb);
        ~Renderer();

        void setRootViewController(const std::shared_ptr<shittygui::ViewController> &newRoot);

        /// Return a pointer to the shittygui screen
        constexpr inline auto &getScreen() {
            return this->screen;
        }

    private:

    private:
        /// Main event loop
        std::weak_ptr<EventLoop> ev;
        /// Framebuffer to render to
        std::shared_ptr<Framebuffer> fb;

        /// Framebuffer swap callback token
        uint32_t cbToken{0};

        /// The shittygui screen
        std::shared_ptr<shittygui::Screen> screen;

};
}

#endif
