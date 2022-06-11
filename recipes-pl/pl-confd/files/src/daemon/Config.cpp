#include <sys/types.h>
#include <pwd.h>

#include <cerrno>
#include <stdexcept>

#include <plog/Log.h>
#include <toml++/toml.h>

#include "Config.h"

std::filesystem::path Config::gStoragePath;
std::vector<Config::AccessDescriptor> Config::gAllowList;

/**
 * @brief Parse configuration file
 *
 * Read the TOML-encoded configuration file at the specified path.
 */
void Config::Read(const std::filesystem::path &path) {
    const auto tbl = toml::parse_file(path.native());

    // data store settings
    const auto storage = tbl["storage"];
    if(!storage || !storage.is_table()) {
        throw std::runtime_error("invalid `storage` key");
    }

    ReadStorage(*storage.as_table());

    // access control
    const auto access = tbl["access"];
    if(!access || !access.is_table()) {
        throw std::runtime_error("invalid `access` key");
    }
}

/**
 * @brief Read storage configuration
 *
 * Assemble the full path to the sqlite database file that stores the configuration data, while
 * checking that the containing directory at least exists.
 */
void Config::ReadStorage(const toml::table &tbl) {
    // get directory
    const std::string dir = tbl["dir"].value_or("");
    gStoragePath = dir;

    if(!std::filesystem::is_directory(gStoragePath)) {
        throw std::runtime_error("invalid storage directory");
    }

    // append the db name to it
    const std::string name = tbl["db"].value_or("");
    if(name.empty()) {
        throw std::runtime_error("invalid storage file");
    }

    gStoragePath /= name;

    PLOG_INFO << "storage path: " << gStoragePath;
}

/**
 * @brief Read access configuration
 *
 * This gets the default access mode, followed by any explicitly allowed keys/key paths.
 */
void Config::ReadAccess(const toml::table &tbl) {
    // default mode

    // allowed accesses
    const auto accesses = tbl["allow"];
    if(!accesses) {
        return;
    } else if(!accesses.is_array_of_tables()) {
        throw std::runtime_error("invalid access.allow key (expected array of tables)");
    }

    auto arr = *accesses.as_array();
    arr.for_each([](auto&& tbl) {
        if(!tbl.is_table()) {
            throw std::runtime_error("invalid access.allow value (expected table)");
        }
        ReadAccessAllow(*tbl.as_table());
    });
}

/**
 * @brief Process a single allow list entry
 *
 * We expect to have some sort of definition of the source (usually, via the `user` key, which can
 * be either an uid or username we look up and convert to uid) and the allowed key paths to access
 * in the form of the `paths` array, which is strings.
 *
 * Key paths can be specified as literal key names, or with a wildcard character to specify all
 * keys under a certain path.
 */
void Config::ReadAccessAllow(const toml::table &tbl) {
    AccessDescriptor desc;

    // get the access specifier
    const auto user = tbl["user"];
    if(!user) {
        throw std::runtime_error("missing access.allow.user key");
    }

    user.visit([&desc](auto&& n) {
        // look up username
        if constexpr (toml::is_string<decltype(n)>) {
            const auto name = *n;

            errno = 0;
            auto pwd = getpwnam(name.c_str());
            if(!pwd && errno) {
                throw std::system_error(errno, std::generic_category(), "getpwnam");
            } else if(!pwd) {
                //PLOG_ERROR << "failed to look up username '" << name << "'!";
            } else {
                desc.user = pwd->pw_uid;
            }
        }
        // literal uid
        else if constexpr (toml::is_integer<decltype(n)>) {
            desc.user = static_cast<uid_t>(*n);
        } else {
            throw std::runtime_error("invalid access.allow.user value (expected integer or string)");
        }
    });

    // allowed key paths
    const auto paths = tbl["paths"];
    if(!paths || !paths.is_array()) {
        throw std::runtime_error("invalid access.allow.paths key (expected array)");
    }

    paths.as_array()->for_each([&desc](auto&& el) {
        if(!el.is_string()) {
            throw std::runtime_error("invalid access.allow.paths value (expected string)");
        }
        desc.allowed.emplace(*(el.as_string()));
    });

    // store it in allow list
    gAllowList.emplace_back(std::move(desc));
}
