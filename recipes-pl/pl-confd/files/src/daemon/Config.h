#ifndef CONFIG_H
#define CONFIG_H

#include <filesystem>
#include <string_view>

#include <toml++/toml.h>

/**
 * @brief Daemon configuration handler
 *
 * This dude holds the configuration of the daemon, including access control lists, based on a
 * TOML-encoded config file.
 */
class Config {
    public:
        static void Read(const std::filesystem::path &path);

    private:
        static void ReadStorage(const toml::table &);

    private:
        /// Path of the database file
        static std::filesystem::path gStoragePath;
};

#endif
