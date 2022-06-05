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

/**
 * @brief Path to standard font
 */
constexpr static const std::string_view kFontStandardPath{
	"/usr/share/fonts/truetype/LiberationSans-Regular.ttf"
};
/**
 * @brief Path to bold font
 */
constexpr static const std::string_view kFontBoldPath{
	"/usr/share/fonts/truetype/LiberationSans-Bold.ttf"
};
/**
 * @brief Path to italic font
 */
constexpr static const std::string_view kFontItalicPath{
	"/usr/share/fonts/truetype/LiberationSans-Italic.ttf"
};

}

#endif
