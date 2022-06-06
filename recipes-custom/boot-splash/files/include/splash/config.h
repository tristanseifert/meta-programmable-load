#ifndef SPLASH_CONFIG_H
#define SPLASH_CONFIG_H

#include <cstdint>
#include <string_view>

/**
 * Configuration for the splash display
 */
namespace Config {
/**
 * @brief Framebuffer device to open
 */
constexpr static const std::string_view kFramebufferDevice{
	"/dev/fb0"
};

}

#endif
