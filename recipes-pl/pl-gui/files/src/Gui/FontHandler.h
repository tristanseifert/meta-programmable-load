#ifndef GUI_FONTHANDLER_H
#define GUI_FONTHANDLER_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Gui {
/**
 * @brief Font type wrapper
 *
 * This is a small wrapper around the LVGL font. It provides RAII abilities for it.
 */
class Font {
    friend class FontHandler;

    public:
        ~Font();

    private:

    private:
        /// underlying lvgl font
        struct _lv_font_t *font{nullptr};
};

/**
 * @brief Font provider
 *
 * This is a wrapper around fontconfig, allowing software to instantiate LVGL fonts by their name
 * rather than full path.
 */
class FontHandler {
    public:
        static void Init();
        static void Deinit();

        /// Get the shared instance of the font handler
        static auto The() {
            return gShared;
        }

        struct _lv_font_t *get(const std::string_view &name, const uint16_t size, const bool bold,
                const bool italic);

    private:
        FontHandler();
        ~FontHandler() = default;

    private:
        /// Shared font handler instance
        static FontHandler *gShared;

        /// fontconfig data
        struct _FcConfig *fconf{nullptr};
};
}

#endif
