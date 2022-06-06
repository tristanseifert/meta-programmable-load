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
            if(this->bannerDirty) {
                this->drawBanner();
                this->bannerDirty = true;
            }

            if(this->progressDirty) {
                this->drawProgressBar();
                this->progressDirty = false;
            }
            if(this->progressStringDirty) {
                this->drawProgressString();
                this->progressStringDirty = false;
            }

            if(this->versionDirty) {
                this->drawVersionStrings();
                this->versionDirty = false;
            }
        }

        /**
         * @brief Draw the background of the splash screen.
         *
         * Typically, this is invoked internally when we've got a dirty region to update, with a
         * clip rect applied; but it's provided here so we can fill the framebuffer on setup.
         */
        void drawBackground();

        /**
         * @brief Update boot progress percentage
         */
        constexpr inline void setProgress(const double newProgress) {
            this->progress = newProgress;
            this->progressDirty = true;
        }
        /**
         * @brief Update progress string
         */
        inline void setProgressString(const std::string_view &newProgressString) {
            this->progressString = newProgressString;
            this->progressStringDirty = true;
        }
        /**
         * @brief Update version string
         */
        inline void setVersion(const std::string_view &newVersion) {
            this->versionString = newVersion;
            this->versionDirty = true;
        }

        /**
         * @brief Check whether the drawer is dirty
         */
        constexpr inline auto isDirty() const {
            return this->bannerDirty | this->progressDirty | this->progressStringDirty
                | this->versionDirty;
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
        constexpr static const double kBannerTextY{0.05};
        /// Color for top banner string
        constexpr static const Color kBannerTextColor{0.9, 0.8, 0.8};

        /// Vertical position of version text
        constexpr static const double kVersionTextY{0.885};
        /// Color for version strings
        constexpr static const Color kVersionTextColor{0.5, 0.5, 0.5};

    private:
        /// Framebuffer to render to
        FbSurface &surface;

        /// Pango text layout object
        struct _PangoLayout *textLayout{nullptr};

        /// is progress bar dirty?
        bool progressDirty{true};
        /// Boot-up progress percentage
        double progress{0.};

        /// is progress message string dirty?
        bool progressStringDirty{true};
        /// Boot-up detail progress message
        std::string progressString{"Please wait..."};
        /// Font description for boot progress text
        struct _PangoFontDescription *progressStringFont{nullptr};

        /// is banner string dirty?
        bool bannerDirty{true};
        /// Banner text to display at top
        std::string banner{"Programmable Load"};
        /// Font description for top banner
        struct _PangoFontDescription *bannerFont{nullptr};

        /// Is version string dirty?
        bool versionDirty{false};
        /// Version string to display
        std::string versionString;
        /// Font description for version font
        struct _PangoFontDescription *versionFont{nullptr};
};

#endif
