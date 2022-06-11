#ifndef CONFIG_H
#define CONFIG_H

#include <sys/types.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <toml++/toml.h>

/**
 * @brief Daemon configuration handler
 *
 * This dude holds the configuration of the daemon, including access control lists, based on a
 * TOML-encoded config file.
 */
class Config {
    public:
        struct AccessDescriptor {
            /// User id
            uid_t user;
            /// keys allowed to access
            std::unordered_set<std::string> allowed;
        };

    public:
        static void Read(const std::filesystem::path &path);

    private:
        static void ReadStorage(const toml::table &);
        static void ReadAccess(const toml::table &);
        static void ReadAccessAllow(const toml::table &);

    private:
        /// Path of the database file
        static std::filesystem::path gStoragePath;
        /// Allowed access list
        static std::vector<AccessDescriptor> gAllowList;
};

#endif
