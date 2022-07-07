#ifndef COPROCESSOR_H
#define COPROCESSOR_H

#include <filesystem>
#include <stdexcept>
#include <string_view>

/**
 * @brief Coprocessor wrapper class
 *
 * This is a thin wrapper around the coproc interface (exported to usermode via sysfs) and serves
 * to load the firmware, and start/stop the processor.
 */
class Coprocessor {
    public:
        /// States for the coprocessor to be in
        enum class State {
            Unknown,
            Running,
            Stopped,
            Crashed,
        };

        void loadFirmware(const std::filesystem::path &fwPath);

        /**
         * @brief Start the coprocessor
         */
        void start() {
            this->setState(State::Running);
        }

        /**
         * @brief Stop the coprocessor
         */
        void stop() {
            this->setState(State::Stopped);
        }

    private:
        /// Set the state of the coprocessor
        void setState(const State newState) {
            switch(newState) {
                case State::Running:
                    this->writeFile(kRprocSysfsBase, "state", "start");
                    break;
                case State::Stopped:
                    this->writeFile(kRprocSysfsBase, "state", "stop");
                    break;

                default:
                    throw std::invalid_argument("invalid coproc state");
            }
        }

        /// Set the directory to search for firmware in
        void setFirmwareDirectory(const std::string_view &directory) {
            this->writeFile(kFirmwareSysfsBase, "path", directory);
        }
        /// Set the firmware file name to load
        void setFirmwareFilename(const std::string_view &filename) {
            this->writeFile(kRprocSysfsBase, "firmware", filename);
        }

        void writeFile(const std::string_view &, const std::string_view &,
                const std::string_view &);

    private:
        /// Path of the "firmware base path" sysfs variable
        constexpr static const std::string_view kFirmwareSysfsBase{"/sys/module/firmware_class/parameters/"};
        /// Remoteproc base path
        constexpr static const std::string_view kRprocSysfsBase{"/sys/class/remoteproc/remoteproc0/"};
};

#endif
