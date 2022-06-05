#include "Drawer.h"
#include "FbSurface.h"

#include <algorithm>

#include <cairo/cairo.h>

/**
 * @brief Draw the progress bar
 */
void Drawer::drawBar() {
    auto ctx = this->surface.getContext();
    const auto clampedProgress = std::max(0., std::min(1., this->progress));

    constexpr static const auto barX{(1.0 - kProgressBarWidth) / 2.};

    // draw background (filled)
    if(this->progress > 0) {
        cairo_rectangle(ctx, barX, kProgressBarY, kProgressBarWidth * clampedProgress,
                this->surface.translateWidth(kProgressBarHeight));

        cairo_set_source_rgb(ctx, std::get<0>(kProgressBarFillColor),
                std::get<1>(kProgressBarFillColor), std::get<2>(kProgressBarFillColor));
        cairo_fill(ctx);
    }

    // draw background (unfilled)
    cairo_rectangle(ctx, barX + (kProgressBarWidth * clampedProgress), kProgressBarY,
            kProgressBarWidth - (clampedProgress * kProgressBarWidth),
            this->surface.translateWidth(kProgressBarHeight));

    cairo_set_source_rgb(ctx, std::get<0>(kProgressBarInteriorColor),
            std::get<1>(kProgressBarInteriorColor), std::get<2>(kProgressBarInteriorColor));
    cairo_fill(ctx);

    // draw outline
    cairo_rectangle(ctx, barX, kProgressBarY, kProgressBarWidth,
            this->surface.translateWidth(kProgressBarHeight));

    cairo_set_line_width(ctx, this->surface.translateWidth(kProgressStrokeWidth));
    cairo_set_source_rgb(ctx, std::get<0>(kProgressStrokeColor), std::get<1>(kProgressStrokeColor),
            std::get<2>(kProgressStrokeColor));

    cairo_stroke(ctx);
}
