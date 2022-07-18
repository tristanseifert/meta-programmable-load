#include <stdexcept>

#include <fmt/format.h>
#include <plog/Log.h>

#include <shittygui/Image.h>

#include "IconManager.h"

using namespace Gui;

std::filesystem::path IconManager::gBasePath;

/**
 * @brief Directory of all icons
 *
 * This contains a mapping of icon type IDs to the information needed to load the icon from the
 * filesystem.
 */
const std::unordered_map<IconManager::Icon, IconManager::Info> IconManager::gIconList{{
    { Icon::NetworkUp,          { "networking_green.png", false, true }},
    { Icon::NetworkDown,        { "networking_red.png", false, true }},
    { Icon::TemperatureLowest,  { "temperature_cold.png", false, true }},
    { Icon::TemperatureLow,     { "temperature_cool.png", false, true }},
    { Icon::TemperatureNormal,  { "temperature_normal.png", false, true }},
    { Icon::TemperatureWarm,    { "temperature_warm.png", false, true }},
    { Icon::TemperatureHot,     { "temperature_hot.png", false, true }},
    { Icon::UsbLogo,            { "usb_3.png", false, true }},
    { Icon::Connected,          { "connect.png", false, true }},
    { Icon::Disconnected,       { "disconnect.png", false, true }},
    { Icon::Cheese,             { "cheese.png", false, true }},
}};


/**
 * @brief Set the base path to search for icons at
 */
void IconManager::SetBasePath(const std::filesystem::path &base) {
    gBasePath = base;

    PLOG_VERBOSE << "Icon base path: " << gBasePath.native();
}

/**
 * @brief Load an the given icon
 */
std::shared_ptr<shittygui::Image> IconManager::LoadIcon(const Icon what, const Size size) {
    // check for it in the cache
    // TODO: implement

    // ensure it's valid and get its info
    if(!gIconList.contains(what)) {
        return nullptr;
    }

    const auto &info = gIconList.at(what);
    if(!SupportsSize(info, size)) {
        return nullptr;
    }

    // build up path and load it
    auto path = gBasePath;
    path /= GetDirectoryName(size);
    path /= info.filename;

    return shittygui::Image::Read(path);
}

