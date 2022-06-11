#include <stdexcept>
#include <iostream>

#include <toml++/toml.h>

#include "Config.h"

std::filesystem::path Config::gStoragePath;

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

    std::cout << "storage path: " << gStoragePath << std::endl;
}
