#include <fcntl.h>
#include <unistd.h>

#include <sstream>
#include <system_error>

#include <plog/Log.h>

#include "Coprocessor.h"

/**
 * @brief Load coprocessor firmware
 *
 * Load the coprocessor's firmware from an ELF file at the specified path.
 */
void Coprocessor::loadFirmware(const std::filesystem::path &fwPath) {
    PLOG_INFO << "loading coproc fw from " << fwPath.native();

    this->setFirmwareDirectory(fwPath.parent_path().native());
    this->setFirmwareFilename(fwPath.filename().native());
}

/**
 * @brief Write a coprocessor management file
 *
 * Write the specified string into a coprocessor management file.
 *
 * @param base Base directory to (or full path of) the management file
 * @param name Name of the management file (or empty if full path is specified)
 * @param value String to write to the file
 */
void Coprocessor::writeFile(const std::string_view &base, const std::string_view &name,
        const std::string_view &value) {
    int fd, err;

    // figure out full path
    std::filesystem::path path(base);

    if(!name.empty()) {
        path /= name;
    }

    PLOG_VERBOSE << "writing coproc file " << path.native() << " = '" << value << "'";

    // open file
    fd = open(path.native().c_str(), O_RDWR);
    if(fd == -1) {
        throw std::system_error(errno, std::generic_category(), "open coproc file");
    }

    // write the value
    try {
        err = write(fd, value.data(), value.length());
        if(err == -1) {
            throw std::system_error(errno, std::generic_category(), "write coproc file");
        }
    } catch(const std::exception &) {
        // ensure file is closed
        close(fd);
        throw;
    }

    // clean up
    close(fd);
}

