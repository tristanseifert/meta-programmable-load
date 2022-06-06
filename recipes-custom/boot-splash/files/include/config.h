#ifndef SPLASH_CONFIG_H
#define SPLASH_CONFIG_H

#include <cstdint>
#include <string_view>

/**
 * Configuration for the splash display
 */
namespace Config {
/**
 * @brief Framebuffer device to render the splash screen on
 */
constexpr static const std::string_view kFramebufferDevice{
    "/dev/fb0"
};

/**
 * @brief Pathname for the UNIX domain socket for control
 *
 * This specifies a name in the Linux abstract namespace. This avoids dependence on the rootfs or
 * anywhere writeable existing. This means the first byte of the name _must_ be \0.
 */
constexpr static const std::string_view kControlSocketPath {
    "/run/bootsplash/control"
};
}

#endif
