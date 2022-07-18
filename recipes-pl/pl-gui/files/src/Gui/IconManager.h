#ifndef GUI_ICONMANAGER_H
#define GUI_ICONMANAGER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <unordered_map>

#include <shittygui/Image.h>

namespace Gui {
/**
 * @brief Icon database
 *
 * This is used to retrieve icons from the filesystem.
 *
 * The file structure is assumed to be identical to that of the `data` directory.
 */
class IconManager {
    public:
        /**
         * @brief Icon size options
         */
        enum class Size: uint8_t {
            /// 16x16 square image
            Square16                            = 1,
            /// 32x32 square image
            Square32                            = 2,
        };

        /**
         * @brief Icon options
         *
         * This enum defines the different icons that we ship with the app. Each entry here should
         * correspond to a matching entry in the global icon list.
         *
         * @seeAlso gIconList
         */
        enum class Icon: uint16_t {
            /// Indicates the network is up (green)
            NetworkUp,
            /// Indicates the network is down (red)
            NetworkDown,

            TemperatureLowest,
            TemperatureLow,
            TemperatureNormal,
            TemperatureWarm,
            TemperatureHot,

            UsbLogo,

            Disconnected,
            Connected,

            /// A block of cheese, lol
            Cheese,
        };

        static void SetBasePath(const std::filesystem::path &base);

        /**
         * @brief Load the given icon
         *
         * The icon is loaded from disk, if we don't already have it in our cache. If it's been
         * cached, simply return that instance.
         */
        static std::shared_ptr<shittygui::Image> LoadIcon(const Icon what, const Size size);

        /**
         * @brief Check if the icon is available in the size requested
         */
        static inline bool HasIcon(const Icon what, const Size size) {
            if(!gIconList.contains(what)) {
                return false;
            }
            const auto &info = gIconList.at(what);

            return SupportsSize(info, size);
        }

    private:
        /**
         * @brief Information structure for an icon
         */
        struct Info {
            /// Filename of the icon on disk
            std::string_view filename;

            /// Is the icon available in 16x16?
            uintptr_t has16Square               :1{false};
            /// Is the icon available in 32x32?
            uintptr_t has32Square               :1{false};
        };

        /**
         * @brief Get the directory name for a given icon size
         */
        constexpr static std::string_view GetDirectoryName(const Size size) {
            switch(size) {
                case Size::Square16:
                    return "16x16";
                case Size::Square32:
                    return "32x32";
            }
        }

        /**
         * @brief Check whether the given icon supports the specified size
         */
        constexpr static inline bool SupportsSize(const Info &info, const Size size) {
            switch(size) {
                case Size::Square16:
                    return info.has16Square;
                case Size::Square32:
                    return info.has32Square;
                default:
                    return false;
            }
        }

    private:
        /// Base path for icons to load from
        static std::filesystem::path gBasePath;
        /// Info for all icons
        static const std::unordered_map<Icon, Info> gIconList;
};
}

#endif
