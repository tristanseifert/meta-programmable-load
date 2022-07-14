#include <filesystem>
#include <stdexcept>
#include <string>

#include <fontconfig/fontconfig.h>
/*#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_CACHE_H
#include FT_SIZES_H
*/

#include <lvgl.h>

#include <fmt/format.h>
#include <plog/Log.h>

#include "FontHandler.h"

using namespace Gui;

FontHandler *FontHandler::gShared{nullptr};

/**
 * @brief Initialize the shared font handler
 */
void FontHandler::Init() {
    if(gShared) {
        throw std::runtime_error("repeated initialization of FontHandler not allowed!");
    }
    gShared = new FontHandler;
}

/**
 * @brief Deinitialize the font handler
 */
void FontHandler::Deinit() {
    if(!gShared) {
        throw std::runtime_error("cannot deinit FontHandler before init!");
    }

    auto temp = gShared;
    gShared = nullptr;
    delete temp;
}

/**
 * @brief Initialize the font handler
 *
 * This initializes the fontconfig library.
 */
FontHandler::FontHandler() {
    // set up fontconfig
    this->fconf = FcInitLoadConfigAndFonts();

    // test
    auto ptr = this->get("Liberation Sans", 16, true, true);
    PLOG_VERBOSE << "dinish 16 = " << (void *) ptr;
}



/**
 * @brief Get a font with the given name and style
 *
 * @param name Font face name (in fontconfig query type)
 * @param size Size, in points, of the desired font
 *
 * @return A font object, or `nullptr` if not found
 */
lv_font_t *FontHandler::get(const std::string_view &name, const uint16_t size, const bool bold,
        const bool italic) {
    FcResult result;
    std::filesystem::path fontPath;

    // parse the fontconfig pattern and perform some basic substitution
    auto pat = FcNameParse(reinterpret_cast<const FcChar8 *>(name.data()));
    if(!pat) {
        throw std::invalid_argument("failed to parse font name");
    }

    if(bold && italic) {
        FcPatternAddString(pat, FC_STYLE, reinterpret_cast<const FcChar8 *>("BoldItalic"));
    } else {
        if(bold) {
            FcPatternAddString(pat, FC_STYLE, reinterpret_cast<const FcChar8 *>("Bold"));
        }
        if(italic) {
            FcPatternAddString(pat, FC_STYLE, reinterpret_cast<const FcChar8 *>("Italic"));
        }
    }

    FcConfigSubstitute(this->fconf, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    // set up a query to get the font family, style, and file path
    auto fs = FcFontSetCreate();
    auto os = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_FILE, nullptr);
    auto fontPatterns = FcFontSort(this->fconf, pat, FcTrue, 0, &result);

    if(!fontPatterns || !fontPatterns->nfont) {
        throw std::runtime_error("fontconfig failed to find any fonts (wtf?)");
    }

    // perform query and select best match
    auto pattern = FcFontRenderPrepare(this->fconf, pat, fontPatterns->fonts[0]);
    if(pattern) {
        FcFontSetAdd(fs, pattern);
    } else {
        throw std::runtime_error("failed to prepare matched font for loading");
    }

    FcFontSetSortDestroy(fontPatterns);
    FcPatternDestroy(pat);

    // retrieve font attributes
    if(fs) {
        if(fs->nfont > 0) {
            FcValue v;
            auto font = FcPatternFilter(fs->fonts[0], os);

            FcPatternGet(font, FC_FILE, 0, &v);
            fontPath = reinterpret_cast<const char *>(v.u.f);

            FcPatternDestroy(font);
        }

        FcFontSetDestroy(fs);
    } else {
        throw std::runtime_error("failed to acquire font set");
    }

    // load the font
    PLOG_VERBOSE << "font path for '" << name << "': " << fontPath;

    // no font found if we get here
    return nullptr;
}
