#include <stdexcept>

#include <event2/event.h>

#include <fmt/format.h>
#include <plog/Log.h>

#include "EventLoop.h"
#include "Framebuffer.h"
#include "Renderer.h"

#include <shittygui/Screen.h>

using namespace Gui;

/**
 * @brief Initialize GUI library
 */
Renderer::Renderer(const std::shared_ptr<EventLoop> &ev, const std::shared_ptr<Framebuffer> &fb) :
    ev(ev), fb(fb) {
    // set up the screen
    const auto &size = this->fb->getSize();
    this->screen = std::make_shared<shittygui::Screen>(shittygui::Screen::PixelFormat::RGB24,
            shittygui::Size(size.first, size.second));

    this->screen->setRotation(shittygui::Screen::Rotation::Rotate270);
    this->screen->setBackgroundColor({0, 0.15, 0});

    // install a swap callback
    this->cbToken = this->fb->addSwapCallback([&](auto bufIdx) {
        // handle animations and draw
        this->screen->handleAnimations();

        if(this->screen->isDirty()) {
            this->screen->redraw();
        }

        // copy the buffer out
        const auto &size = this->fb->getSize();
        auto dispFb = this->fb->getData(bufIdx);

        auto inPtr = reinterpret_cast<const uint32_t *>(this->screen->getBuffer());
        auto outPtr = reinterpret_cast<uint32_t *>(dispFb.data());
        memcpy(outPtr, inPtr,
                static_cast<size_t>(size.first) * static_cast<size_t>(size.second) * 4);
    });
}

/**
 * @brief Tear down GUI library
 */
Renderer::~Renderer() {
    this->fb->removeSwapCallback(this->cbToken);
}

/**
 * @brief Set the root view controller displayed on the screen
 */
void Renderer::setRootViewController(const std::shared_ptr<shittygui::ViewController> &newRoot) {
    this->screen->setRootViewController(newRoot);
}
