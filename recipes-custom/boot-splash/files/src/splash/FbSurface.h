#ifndef SPLASH_FBSURFACE_H
#define SPLASH_FBSURFACE_H

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <cairo/cairo.h>

/**
 * @brief Maps a framebuffer as a Cairo surface
 *
 * This handles opening a framebuffer and mapping it as a Cairo surface, so that we can do drawing
 * on it with Cairo. It also establishes a Cairo context for the surface.
 */
class FbSurface {
    public:
        /**
         * @brief Open framebuffer at the given path and create drawing surface
         */
        FbSurface(const std::string_view path) {
            // open fb and map its contents
            this->fd = open(path.data(), O_RDWR);
            if(this->fd == -1) {
                throw std::system_error(errno, std::generic_category(), "open fb");
            }

            struct fb_fix_screeninfo finfo;
            UpdateFixedInfo(this->fd, finfo);

            this->base = mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd, 0);
            if(this->base == reinterpret_cast<void *>(-1)) {
                throw std::system_error(errno, std::generic_category(), "map fb");
            }

            // acquire buffer info
            UpdateVariableInfo(this->fd, this->info);

            // now create a cairo surface
            cairo_format_t format;

            switch(this->info.bits_per_pixel) {
                case 16:
                    format = CAIRO_FORMAT_RGB16_565;
                    break;

                // TODO: this handling is definitely incomplete and incorrect
                default:
                    throw std::runtime_error("unsupported pixel format");
            }

            this->surface = cairo_image_surface_create_for_data(
                    reinterpret_cast<unsigned char *>(this->base), format, this->info.xres,
                    this->info.yres, finfo.line_length);

            if(!this->surface) {
                throw std::runtime_error("failed to create surface");
            }

            // create the context
            this->ctx = cairo_create(this->surface);
            if(cairo_status(this->ctx) != CAIRO_STATUS_SUCCESS) {
                throw std::runtime_error(cairo_status_to_string(cairo_status(this->ctx)));
            }

            // rotate it to fit 
            // XXX: hardcode it (fbdev broken?)
            this->info.rotate = FB_ROTATE_CCW;

            switch(this->info.rotate) {
                // 90°
                case FB_ROTATE_CW:
                    cairo_rotate(this->ctx, M_PI_2);
                    break;
                // 180°
                case FB_ROTATE_UD:
                    cairo_rotate(this->ctx, M_PI);
                    break;
                // 270°
                case FB_ROTATE_CCW:
                    cairo_rotate(this->ctx, -M_PI_2);
                    break;
                // no rotation by default
                default:
                    break;
            }
            // flip X axis
            cairo_translate(this->ctx, -800, 0);
        }

        /**
         * @brief Clean up all resources associated with the surface.
         */
        ~FbSurface() {
            // destruct the surface and drawing context
            if(this->surface) {
                cairo_surface_destroy(this->surface);
            }

            if(this->ctx) {
                cairo_destroy(this->ctx);
            }

            // close framebuffer fd (which unmaps it as well)
            if(this->fd > 0) {
                close(this->fd);
            }
        }

        /**
         * @brief Get Cairo surface for framebuffer
         */
        constexpr inline auto getSurface() const {
            return this->surface;
        }
        /**
         * @brief Get cairo drawing context
         */
        constexpr inline auto getContext() const {
            return this->ctx;
        }

        /**
         * @brief Get framebuffer width
         */
        constexpr inline size_t getFbWidth() const {
            return this->info.xres;
        }
        /**
         * @brief Get framebuffer height
         */
        constexpr inline size_t getFbHeight() const {
            return this->info.yres;
        }

        /**
         * @brief Clear the framebuffer to ther specified color.
         */
        void clear(const double r, const double g, const double b) {
            cairo_set_source_rgb(this->ctx, r, g, b);
            cairo_paint(this->ctx);

            cairo_surface_flush(this->surface);
        }

        /**
         * @brief Translate height to user coordinate space
         */
        inline double translateHeight(const double in) const {
            return in;
            /*
            double temp{in}, unused{0};
            cairo_device_to_user_distance(this->ctx, &unused, &temp);
            return temp;
            */
        }

        /**
         * @brief Translate line width to user coordinate space
         */
        inline double translateWidth(const double in) const {
            return in;
            /*
            double temp{in}, unused{0};
            cairo_device_to_user_distance(this->ctx, &temp, &unused);
            return temp;
            */
        }

    private:
        /**
         * @brief Update framebuffer fixed info (buffer size)
         */
        static inline void UpdateFixedInfo(int fd, struct fb_fix_screeninfo &info) {
            if(ioctl(fd, FBIOGET_FSCREENINFO, &info) == -1) {
                throw std::system_error(errno, std::generic_category(), "FBIOGET_FSCREENINFO");
            }
        }

        /**
         * @brief Update framebuffer variable info (resolution)
         */
        static inline void UpdateVariableInfo(int fd, struct fb_var_screeninfo &info) {
            if(ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1) {
                throw std::system_error(errno, std::generic_category(), "FBIOGET_VSCREENINFO");
            }
        }

    private:
        /// File descriptor for the framebuffer
        int fd{-1};
        /// Memory area into which the framebuffer is mapped
        void *base{nullptr};
        /// Framebuffer information
        struct fb_var_screeninfo info;

        /// Cairo surface created over the framebuffer
        cairo_surface_t *surface{nullptr};
        /// Drawing context
        cairo_t *ctx{nullptr};
};

#endif
