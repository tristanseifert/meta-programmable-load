#ifndef CONFIG_H
#define CONFIG_H

#include <sys/types.h>

#include <filesystem>
#include <optional>
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
        /**
         * @brief Describe a set of rules applying to a connected client
         *
         * This consists of two components: first, the identifiers (user id, group id, or both; if
         * both are specified, both must match) and then the set of keys (or key spaces)
         */
        struct AccessDescriptor {
            /// User id
            std::optional<uid_t> user;
            /// Group id
            std::optional<gid_t> group;

            /// keys allowed to access
            std::unordered_set<std::string> allowed;
        };

    public:
        static void Read(const std::filesystem::path &path);

        /// Get the path for the RPC listening socket
        static const auto &GetRpcSocketPath() {
            return gSocketPath;
        }
        /// Get the path of the storage database
        static const auto &GetStoragePath() {
            return gStoragePath;
        }

    private:
        static void ReadRpc(const toml::table &);
        static void ReadStorage(const toml::table &);
        static void ReadAccess(const toml::table &);
        static void ReadAccessAllow(const toml::table &);

    private:
        /// Path to the UNIX domain socket used for RPC
        static std::filesystem::path gSocketPath;
        /// Path of the database file
        static std::filesystem::path gStoragePath;
        /// Allowed access list
        static std::vector<AccessDescriptor> gAllowList;
};

#endif
