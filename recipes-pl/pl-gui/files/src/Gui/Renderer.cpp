#include <stdexcept>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_CACHE_H
#include FT_SIZES_H

#include <event2/event.h>

#include <fmt/format.h>
#include <plog/Log.h>

#include "EventLoop.h"
#include "Framebuffer.h"
#include "Renderer.h"

#include <shittygui/Screen.h>
#include <shittygui/Widgets/Container.h>
#include <shittygui/Widgets/Label.h>
#include <shittygui/Widgets/ProgressBar.h>

using namespace Gui;

static void InitScreen(const std::shared_ptr<shittygui::Screen> &screen) {
    // create outer container
    auto cont = shittygui::MakeWidget<shittygui::widgets::Container>({0, 0}, {800, 480});
    cont->setDrawsBorder(false);
    cont->setBorderRadius(0.);
    cont->setBackgroundColor({0, 0.125, 0});

    // left container
    auto left = shittygui::MakeWidget<shittygui::widgets::Container>({20, 20}, {360, 430});
    left->setBackgroundColor({0.33, 0, 0});
    cont->addChild(left);

    auto leftLabel = shittygui::MakeWidget<shittygui::widgets::Label>({2, 0}, {356, 45},
            "Hello World!");
    leftLabel->setFont("Avenir Next Bold", 24);
    leftLabel->setTextAlign(shittygui::TextAlign::Center);
    leftLabel->setTextColor({1, 1, 1});
    left->addChild(leftLabel);

    auto longLabel = shittygui::MakeWidget<shittygui::widgets::Label>({3, 45}, {354, 240});
    longLabel->setContent(R"(I'm baby retro single-origin coffee stumptown small batch echo park, chicharrones tote bag vexillologist literally. Mlkshk intelligentsia shabby chic sustainable. Shabby chic copper mug helvetica DIY art party you probably haven't heard of them, humblebrag cloud bread adaptogen blog. Dreamcatcher wayfarers raw denim XOXO lyft disrupt jianbing tattooed 90's chia. Gluten-free post-ironic bushwick single-origin coffee brooklyn yes plz. Umami humblebrag shabby chic, selvage pok pok franzen church-key.
Lomo photo booth single-origin coffee health goth raclette YOLO franzen unicorn vexillologist migas woke wolf irony. Retro ugh palo santo cray aesthetic fashion axe, pabst hashtag poutine. Meggings tbh schlitz, mixtape celiac viral la croix hammock offal squid brooklyn yr fam. Vice chambray kogi fashion axe selfies schlitz trust fund yes plz. Keytar lo-fi affogato pop-up slow-carb schlitz drinking vinegar cray pinterest. Fashion axe vice messenger bag scenester cold-pressed XOXO schlitz YOLO kombucha you probably haven't heard of them. Direct trade small batch pickled, enamel pin yes plz lumbersexual chartreuse forage iceland messenger bag prism.)");
    longLabel->setFont("Liberation Sans", 11);
    longLabel->setTextAlign(shittygui::TextAlign::Left);
    longLabel->setWordWrap(true);
    longLabel->setEllipsizeMode(shittygui::EllipsizeMode::Middle);
    longLabel->setTextColor({0.9, 1, 1});

    left->addChild(longLabel);


    // right container
    auto right = shittygui::MakeWidget<shittygui::widgets::Container>({420, 20}, {360, 430});
    right->setBackgroundColor({0, 0, 0.33});

    auto indetBar = shittygui::MakeWidget<shittygui::widgets::ProgressBar>({5, 400}, {350, 22},
            shittygui::widgets::ProgressBar::Style::Indeterminate);
    right->addChild(indetBar);

    auto normalBar = shittygui::MakeWidget<shittygui::widgets::ProgressBar>({5, 368}, {350, 22},
            shittygui::widgets::ProgressBar::Style::Determinate);
    normalBar->setProgress(.5);
    right->addChild(normalBar);

    cont->addChild(right);

    screen->setRootWidget(cont);
}

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
    this->screen->setBackgroundColor({1, 0, 0});

    InitScreen(this->screen);

    // install a swap callback (TODO: take the result)
    this->fb->addSwapCallback([&](auto bufIdx) {
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

}
