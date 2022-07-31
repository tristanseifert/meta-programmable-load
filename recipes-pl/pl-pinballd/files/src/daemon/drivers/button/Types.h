#ifndef DRIVERS_BUTTON_TYPES_H
#define DRIVERS_BUTTON_TYPES_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace drivers::button {
/**
 * @brief Buttons available on the system
 */
enum class Button: uintptr_t {
    ModeCc                              = 0x10,
    ModeCv                              = 0x11,
    ModeCw                              = 0x12,
    ModeExt                             = 0x13,
    LoadOn                              = 0x20,
    Menu                                = 0x40,
    /// Select (enter) button; usually encoder middle button
    Select                              = 0x41,
};

/**
 * @brief Mapping between button types and their name
 */
static const std::unordered_map<Button, std::string_view> kButtonNames{{
    { Button::ModeCc,   "modeCc" },
    { Button::ModeCv,   "modeCv" },
    { Button::ModeCw,   "modeCw" },
    { Button::ModeExt,  "modeExt" },
    { Button::LoadOn,   "loadOn" },
    { Button::Menu,     "menu" },
    { Button::Select,   "select" },
}};
}

#endif
