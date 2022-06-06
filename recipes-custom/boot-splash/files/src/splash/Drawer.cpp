#include "Drawer.h"
#include "FbSurface.h"

#include <algorithm>
#include <cstdio>

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

/// Helper to set source color
static inline void SetSourceColor(cairo_t *ctx, const Drawer::Color &color) {
    cairo_set_source_rgb(ctx, std::get<0>(color), std::get<1>(color), std::get<2>(color));
}


/**
 * @brief Release all resources.
 */
Drawer::~Drawer() {
    // release the Pango resources
    g_object_unref(this->textLayout);

    pango_font_description_free(this->progressStringFont);
    pango_font_description_free(this->bannerFont);
    pango_font_description_free(this->versionFont);
}

/**
 * @brief Draw background of screen
 *
 * For now, this just fills the screen with a solid color.
 */
void Drawer::drawBackground() {
    auto ctx = this->surface.getContext();

    // create gradient
    auto pattern = cairo_pattern_create_linear(0.5, 0.1, 0.5, 0.9);

    cairo_pattern_add_color_stop_rgb(pattern, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgb(pattern, 1, 0, 0, 0.2);

    // fill with it
    cairo_set_source(ctx, pattern);
    cairo_paint(ctx);

    // clean up
    cairo_pattern_destroy(pattern);
}

/**
 * @brief Draw top banner
 */
void Drawer::drawBanner() {
    auto ctx = this->surface.getContext();

    // establish clipping rect (XXX: don't hardcode line height) and fill background
    cairo_save(ctx);
    cairo_rectangle(ctx, 0, kBannerTextY, 1, this->surface.translateHeight(48));
    cairo_clip(ctx);

    this->drawBackground();

    // draw text
    SetSourceColor(ctx, kBannerTextColor);
    cairo_move_to(ctx, 0.5, kBannerTextY);

    this->renderText(this->banner, this->bannerFont, TextAlignment::Center);

    cairo_restore(ctx);
}

/**
 * @brief Draw the progress bar
 *
 * This doesn't have to draw the background under as it's fully opaque.
 */
void Drawer::drawProgressBar() {
    auto ctx = this->surface.getContext();
    const auto clampedProgress = std::max(0., std::min(1., this->progress));

    constexpr static const auto barX{(1.0 - kProgressBarWidth) / 2.};

    // draw background (filled)
    if(this->progress > 0) {
        cairo_rectangle(ctx, barX, kProgressBarY, kProgressBarWidth * clampedProgress,
                this->surface.translateHeight(kProgressBarHeight));

        SetSourceColor(ctx, kProgressBarFillColor);
        cairo_fill(ctx);
    }

    // draw background (unfilled)
    cairo_rectangle(ctx, barX + (kProgressBarWidth * clampedProgress), kProgressBarY,
            kProgressBarWidth - (clampedProgress * kProgressBarWidth),
            this->surface.translateHeight(kProgressBarHeight));

    SetSourceColor(ctx, kProgressBarInteriorColor);
    cairo_fill(ctx);

    // draw outline
    cairo_rectangle(ctx, barX, kProgressBarY, kProgressBarWidth,
            this->surface.translateHeight(kProgressBarHeight));

    cairo_set_line_width(ctx, this->surface.translateHeight(kProgressStrokeWidth));
    SetSourceColor(ctx, kProgressStrokeColor);

    cairo_stroke(ctx);
}

/**
 * @brief Draw boot progress string
 *
 * This is a single line of text that appears right above the progress bar. It can be used to
 * provide additional information about the current phase of boot.
 */
void Drawer::drawProgressString() {
    const auto kProgressTextY{kProgressBarY - this->surface.translateHeight(30)};
    auto ctx = this->surface.getContext();

    // establish clipping rect (XXX: don't hardcode line height) and fill background
    cairo_save(ctx);
    cairo_rectangle(ctx, 0, kProgressTextY, 1, this->surface.translateHeight(26));
    cairo_clip(ctx);

    this->drawBackground();

    // draw string
    SetSourceColor(ctx, kProgressTextColor);

    cairo_move_to(ctx, 0.5, kProgressTextY);
    this->renderText(this->progressString, this->progressStringFont, TextAlignment::Center);

    cairo_restore(ctx);
}

/**
 * @brief Draw the version strings
 */
void Drawer::drawVersionStrings() {
    auto ctx = this->surface.getContext();
    constexpr static const auto kX{(1.0 - kProgressBarWidth) / 2.};

    // establish clipping rect (XXX: don't hardcode line height) and fill background
    cairo_save(ctx);
    cairo_rectangle(ctx, kX, kVersionTextY, kProgressBarWidth, 1. - kVersionTextY);
    cairo_clip(ctx);

    this->drawBackground();

    // early out if version isn't known yet
    if(this->versionString.empty()) {
        goto done;
    }

    // versions are left aligned with bar
    SetSourceColor(ctx, kVersionTextColor);
    cairo_move_to(ctx, kX, kVersionTextY);

    this->renderText(this->versionString, this->versionFont, TextAlignment::Left);

    // restore drawing state
done:;
    cairo_restore(ctx);
}



/**
 * @brief Load the required fonts.
 *
 * This creates font descriptors for the fonts we need.
 */
void Drawer::loadFonts() {
    // prepare our text layout engine
    this->textLayout = pango_cairo_create_layout(this->surface.getContext());

    // get font descriptions
    this->bannerFont = pango_font_description_from_string("DINishExpanded Bold 32");
    this->progressStringFont = pango_font_description_from_string("Liberation Sans Italic 16");
    this->versionFont = pango_font_description_from_string("Liberation Sans Regular 11");
}

/**
 * @brief Render aligned text at the current coordinate.
 *
 * @param str UTF-8 encoded string to render
 * @param font Font to render the string with
 * @param align Text alignment, default left align
 *
 * @remark The current coordinate refers to the top of the string, and the left, center, or right
 *         edge based on the text alignment.
 */
void Drawer::renderText(const std::string_view &str, PangoFontDescription *font,
        const TextAlignment align) {
    auto ctx = this->surface.getContext();

    // reset the scale transform (this fucks with font rendering real bad; but kinda hacky)
    cairo_save(ctx);
    cairo_scale(ctx, 1. / static_cast<double>(this->surface.getFbWidth()),
            1. / static_cast<double>(this->surface.getFbHeight()));

    int width, height;

    // fill in the new text string and font description
    pango_layout_set_text(this->textLayout, str.data(), -1);
    pango_layout_set_font_description(this->textLayout, font);

    // apply layout parameters
    switch(align) {
        case TextAlignment::Left:
            pango_layout_set_alignment(this->textLayout, PANGO_ALIGN_LEFT);
            break;
        case TextAlignment::Center:
            pango_layout_set_alignment(this->textLayout, PANGO_ALIGN_CENTER);
            break;
        case TextAlignment::Right:
            pango_layout_set_alignment(this->textLayout, PANGO_ALIGN_RIGHT);
            break;
    }

    // lay out the text
    pango_cairo_update_layout(ctx, this->textLayout);
    pango_layout_get_size(this->textLayout, &width, &height);

    // draw it (after applying an appropriate offset)
    double pX, pY;
    cairo_get_current_point(ctx, &pX, &pY);

    switch(align) {
        case TextAlignment::Left:
            //pY += -(static_cast<double>(height) / PANGO_SCALE);
            break;
        case TextAlignment::Center:
            pX += -(static_cast<double>(width) / PANGO_SCALE) / 2.;
            //pY += -(static_cast<double>(height) / PANGO_SCALE);
            break;
        case TextAlignment::Right:
            //pY += -(static_cast<double>(height) / PANGO_SCALE);
            break;
    }

    cairo_move_to(ctx, pX, pY);
    pango_cairo_show_layout(ctx, this->textLayout);

    // restore state (transforms)
    cairo_restore(ctx);
}
