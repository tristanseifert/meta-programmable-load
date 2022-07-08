#ifndef COPROCESSOR_H
#define COPROCESSOR_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Coprocessor wrapper class
 *
 * This is a thin wrapper around the coproc interface (exported to usermode via sysfs) and serves
 * to load the firmware, and start/stop the processor.
 */
class Coprocessor {
    public:
        ~Coprocessor();

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

        void initRpc();

    private:
        /**
         * @brief Information for an RPC channel endpoint to establish during initialization
         *
         * An array of these structs is specified as `kRpcChannels` and consulted by initRpc()
         * to set up the endpoints. They are identified by the combination of the name and address
         * to the system.
         */
        struct EndpointInfo {
            /// Endpoint name
            const std::string_view name;
            /// Endpoint address (fixed in firmware)
            uint32_t address;
            /// Is this the load control endpoint?
            uint32_t isLoadControl:1{0};
            /// Can this endpoint be directly retrieved by another task?
            uint32_t isRetrievable:1{0};
        };

        /**
         * @brief Information for a connected RPC channel
         */
        struct RpcChannelInfo {
            /// name of the endpoint (used during connection)
            const std::string_view epName;
            /// path to the chardev
            std::filesystem::path chrdevPath;
            /// chardev file descriptor
            int chrdevFd{-1};

            /// can this channel be retrieved via RPC?
            bool isRetrievable{false};
        };

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

            this->coprocState = newState;
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

        void connectRpcEndpoint(const std::string_view &name, const uint32_t address,
                std::filesystem::path &outChrdevPath);

        size_t destroyAllRpcEndpoints();
        void destroyRpcEndpoint(const std::filesystem::path &chrdevPath);
        void destroyRpcEndpoint(const int fd);

    private:
        /// Number of endpoints to establish devices for
        constexpr static const size_t kNumRpcEndpoints{2};
        /// RPC endpoints to establish during connection
        constexpr static const std::array<EndpointInfo, kNumRpcEndpoints> kRpcChannels{{
            /// load control (consumed by loadd)
            {
                .name = "pl.control",
                .address = 0x420,
                .isLoadControl = true,
                .isRetrievable = false,
            },
            /// Interface to confd
            {
                .name = "confd",
                .address = 0x421,
                .isLoadControl = false,
                .isRetrievable = true,
            },
        }};

        /// Path of the "firmware base path" sysfs variable
        constexpr static const std::string_view kFirmwareSysfsBase{"/sys/module/firmware_class/parameters/"};
        /// Remoteproc base path
        constexpr static const std::string_view kRprocSysfsBase{"/sys/class/remoteproc/remoteproc0/"};

        /// rpmsg control device file
        constexpr static const std::string_view kRpmsgCtrlDev{"/dev/rpmsg_ctrl0"};
        /// base path of the rpmsg chrdev files
        constexpr static const std::string_view kRpmsgChrdevBase{"/dev/rpmsg"};

        /// file descriptor to the rpmsg control interface
        int rpmsgCtrlFd{-1};
        /// most recently set coprocessor state
        State coprocState{State::Unknown};

        /// initialized RPC channels
        std::vector<RpcChannelInfo> rpcChannels;
};

#endif
