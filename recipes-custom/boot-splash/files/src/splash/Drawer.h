#ifndef SPLASH_DRAWER_H
#define SPLASH_DRAWER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
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
        Drawer(FbSurface &surface) : surface(surface) {
            this->loadFonts();
        }

        ~Drawer();

        /**
         * @brief Render the splash screen.
         *
         * This will draw the progress bar, any icons, message text and various version info.
         */
        void draw() {
            this->drawBanner();
            this->drawProgressBar();
            this->drawProgressString();
            this->drawVersionStrings();
        }

        /**
         * @brief Update boot progress percentage
         */
        constexpr inline void setProgress(const double newProgress) {
            this->progress = newProgress;
        }
        /**
         * @brief Update version string
         */
        inline void setVersion(const std::string_view &newVersion) {
            this->versionString = newVersion;
        }

    private:
        enum class TextAlignment {
            Left, Center, Right
        };

        void loadFonts();
        void renderText(const std::string_view &, struct _PangoFontDescription *,
                const TextAlignment = TextAlignment::Left);

        void drawBanner();
        void drawProgressBar();
        void drawProgressString();
        void drawVersionStrings();

    /// Configuration
    private:
        /// Width of progress bar (percentage [0, 1])
        constexpr static const double kProgressBarWidth{0.8};
        /// Vertical position of progress bar
        constexpr static const double kProgressBarY{0.825};

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

        /// Color for boot progress string
        constexpr static const Color kProgressTextColor{0.85, 0.85, 0.85};

        /// Vertical position of top banner text
        constexpr static const double kBannerTextY{0.1};
        /// Color for top banner string
        constexpr static const Color kBannerTextColor{0.9, 0.8, 0.8};

        /// Vertical position of version text
        constexpr static const double kVersionTextY{0.92};
        /// Color for version strings
        constexpr static const Color kVersionTextColor{0.5, 0.5, 0.5};

    private:
        /// Framebuffer to render to
        FbSurface &surface;

        /// Pango text layout object
        struct _PangoLayout *textLayout{nullptr};

        /// Boot-up progress percentage
        double progress{0.};
        /// Boot-up detail progress message
        std::string progressString{"Please wait..."};
        /// Font description for boot progress text
        struct _PangoFontDescription *progressStringFont{nullptr};

        /// Banner text to display at top
        std::string banner{"Programmable Load"};
        /// Font description for top banner
        struct _PangoFontDescription *bannerFont{nullptr};

        std::string versionString;
        /// Font description for version font
        struct _PangoFontDescription *versionFont{nullptr};
};

#endif
