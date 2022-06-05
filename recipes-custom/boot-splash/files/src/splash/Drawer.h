#ifndef SPLASH_DRAWER_H
#define SPLASH_DRAWER_H

#include <cstddef>
#include <cstdint>
#include <tuple>

class FbSurface;

/**
 * @brief Handles rendering the boot splash
 *
 * Given a framebuffer surface, this class handles all of the drawing to the framebuffer.
 */
class Drawer {
    public:
        /// RGB color
        using Color = std::tuple<double, double, double>;

    public:
        /**
         * @brief Initialize drawer for the given surface
         */
        Drawer(FbSurface &surface) : surface(surface) {}

        /**
         * @brief Render the splash screen.
         *
         * This will draw the progress bar, any icons, message text and various version info.
         */
        void draw() {
            this->drawBar();
        }

        /**
         * @brief Update boot progress percentage
         */
        constexpr inline void setProgress(const double newProgress) {
            this->progress = newProgress;
        }

    private:
        void drawBar();

    /// Configuration
    private:
        /// Width of progress bar (percentage [0, 1])
        constexpr static const double kProgressBarWidth{0.8};
        /// Vertical position of progress bar
        constexpr static const double kProgressBarY{0.85};

        /// Height of progress bar, pixels
        constexpr static const size_t kProgressBarHeight{16};
        /// Color for progress bar interior
        constexpr static const Color kProgressBarInteriorColor{0, 0, 0};
        /// Color for progress bar fill
        constexpr static const Color kProgressBarFillColor{0.85, 0.2, 0.2};
        /// Width of progress bar stroke, pixels
        constexpr static const size_t kProgressStrokeWidth{1};
        /// Color for progress bar stroke
        constexpr static const Color kProgressStrokeColor{0.6, 0.15, 0.15};

    private:
        /// Framebuffer to render to
        FbSurface &surface;

        /// Boot-up progress
        double progress{0.5};
};

#endif
