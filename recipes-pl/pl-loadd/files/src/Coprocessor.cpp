#include <fcntl.h>
#include <unistd.h>
#include <linux/rpmsg.h>
#include <sys/ioctl.h>

#include <cerrno>
#include <cstring>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>

#include <fmt/format.h>
#include <plog/Log.h>

#include "ConfdEpHandler.h"
#include "ControlEpHandler.h"
#include "Coprocessor.h"

const std::array<Coprocessor::EndpointInfo, Coprocessor::kNumRpcEndpoints> Coprocessor::kRpcChannels{{
    /// load control (consumed by loadd)
    {
        .name = "pl.control",
        .address = 0x420,
        .isLoadControl = true,
        .isRetrievable = false,
        .makeHandler = [](auto fd, auto lrpc, auto outHandler) {
            outHandler = std::make_shared<ControlEpHandler>(fd, lrpc);
        },
    },
    /// Interface to confd
    {
        .name = "confd",
        .address = 0x421,
        .isLoadControl = false,
        .isRetrievable = false,
        .makeHandler = [](auto fd, auto lrpc, auto outHandler) {
            outHandler = std::make_shared<ConfdEpHandler>(fd, lrpc);
        },
    },
}};



/**
 * @brief Clean up coprocessor resources
 *
 * We'll stop the coprocessor (if not already done) then deallocate endpoints and close any file
 * descriptors we still have open. Any external applications that have opened the rpmsg channels
 * will be interrupted when they're killed.
 */
Coprocessor::~Coprocessor() {
    // shut down the coproc; ignore any errors though
    try {
        if(this->coprocState == State::Running) {
            this->stop();
        }
    } catch(const std::exception &e) {
        PLOG_ERROR << "failed to stop coprocessor: " << e.what();
    }

    // simply close all open endpoint descriptors
    for(auto it = this->rpcChannels.rbegin(); it != this->rpcChannels.rend(); ++it) {
        auto &info = *it;

        info.handler.reset();

        if(info.chrdevFd != -1) {
            close(info.chrdevFd);
        }
    }
}

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



/**
 * @brief Initialize RPC communications
 *
 * We'll take advantage of the `rpmsg_chrdev` driver to create character devices for all of the
 * supported RPC endpoints, as defined in the `kRpcChannels` array.
 *
 * Once the channels are set up, open file descriptors for all of them. One of the channels will be
 * managed and handled internally by our event loop (this is the main load-specific message
 * channel, which we re-export with an RPC interface on a domain socket) while the others will just
 * be chilling, until a task checks in and requests it.
 */
void Coprocessor::initRpc(const std::shared_ptr<RpcServer> &lrpc) {
    // close any endpoints that are still open (leftovers)
    try {
        const auto numClosed = this->destroyAllRpcEndpoints();
        PLOG_WARNING_IF(!!numClosed) << "destroyed " << numClosed << " leftover chrdev ep's";
    } catch(const std::exception &e) {
        PLOG_WARNING << "failed to destroy endpoints: " << e.what();
    }

    // open control device if needed
    if(this->rpmsgCtrlFd == -1) {
        PLOG_DEBUG << "opening rpmsg_ctrl at " << kRpmsgCtrlDev;

        this->rpmsgCtrlFd = open(kRpmsgCtrlDev.data(), O_RDWR);
        if(this->rpmsgCtrlFd == -1) {
            throw std::system_error(errno, std::generic_category(), "open rpmsg_ctrl");
        }
    }

    // initialize each channel
    for(const auto &detail : kRpcChannels) {
        int fd{-1};

        try {
            std::filesystem::path devPath;
            std::shared_ptr<EndpointHandler> handler;

            this->connectRpcEndpoint(detail.name, detail.address, devPath);
            PLOG_DEBUG << "opened endpoint " << fmt::format("{}:{:x}", detail.name, detail.address)
                       << " = " << devPath.native();

            // try to open the device
            fd = open(devPath.native().c_str(), O_RDWR);
            if(fd == -1) {
                throw std::system_error(errno, std::generic_category(), "open rpmsg_chrdev");
            }

            // if a handler class was specified for the channel, instantiate it
            if(detail.makeHandler) {
                // provide detailed logging if handler init fails
                try {
                    detail.makeHandler(fd, lrpc, handler);

                    if(!handler) {
                        throw std::runtime_error(fmt::format("invalid handler for '{}'", detail.name));
                    }
                } catch(const std::exception &e) {
                    PLOG_ERROR << "makeHandler failed: " << e.what();

                    throw;
                }
            }

            // insert an info struct for it
            this->rpcChannels.emplace_back(RpcChannelInfo{
                .epName = detail.name,
                .chrdevPath = devPath,
                .chrdevFd = fd,
                .isRetrievable = !!detail.isRetrievable,
                .handler = handler,
            });
        } catch(const std::exception &e) {
            PLOG_FATAL << "failed to initialize rpc endpoint '" << detail.name << "': "
                       << e.what();

            if(fd != -1) {
                // ensure we don't leak the fd (if handler can't initialize)
                close(fd);
            }

            throw;
        }
    }
}

/**
 * @brief Create a character device for the given endpoint
 *
 * Invoke an ioctl on the rpmsg_ctrl device to create a new character device corresponding to an
 * RPC endpoint with the given name and address.
 *
 * @param name ns name the service shall be advertised under
 * @param address Numeric endpoint address of the service on the M4 side
 * @param outChrdevPath Output variable to receive the path of the chardev
 */
void Coprocessor::connectRpcEndpoint(const std::string_view &name, const uint32_t address,
        std::filesystem::path &outChrdevPath) {
    int err;
    struct rpmsg_endpoint_info ept{};
    size_t nextChrdevNum{0};

    // figure out the highest chardev number available (aka the path of this next chardev)
    const std::regex kNameRegex{"rpmsg(\\d+)"};
    for(const auto &dent : std::filesystem::directory_iterator{"/dev/"}) {
        std::smatch m;
        if(!std::regex_match(dent.path().filename().native(), m, kNameRegex) || m.empty()) {
            continue;
        }

        // extract the digit and compare
        const auto &sm = m[1];
        const auto num = std::stoul(sm.str());

        if(num >= nextChrdevNum) {
            nextChrdevNum = num + 1;
        }
    }

    // create endpoint
    ept.src = -1;
    ept.dst = address;
    strncpy(ept.name, name.data(), sizeof(ept.name));
    ept.name[sizeof(ept.name) - 1] = '\0';

    err = ioctl(this->rpmsgCtrlFd, RPMSG_CREATE_EPT_IOCTL, &ept);
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "RPMSG_CREATE_EPT_IOCTL");
    }

    // validate the device was created where we expected it to
    std::filesystem::path path{kRpmsgChrdevBase};
    path += std::to_string(nextChrdevNum);

    if(!std::filesystem::exists(path)) {
        throw std::runtime_error("rpmsg_chrdev not created where we expected!");
    } else if(!std::filesystem::is_character_file(path)) {
        throw std::runtime_error("rpmsg_chrdev is not a character device!");
    }

    outChrdevPath = path;
}


/**
 * @brief Close any open RPC endpoints
 *
 * We'll iterate first through any endpoints we opened (and thus have the paths for) and then
 * iterate through all /dev/rpmsg* devices left over in /dev to destroy.
 *
 * @return Number of endpoints destroyed
 */
size_t Coprocessor::destroyAllRpcEndpoints() {
    size_t count{0};

    // start with any stored endpoints
    for(auto it = this->rpcChannels.rbegin(); it != this->rpcChannels.rend(); ++it) {
        const auto &info = *it;
        PLOG_VERBOSE << "destroying endpoint " << info.epName << " (" << info.chrdevPath.native()
                   << ")";

        try {
            this->destroyRpcEndpoint(info.chrdevFd);
            count++;
        } catch(const std::exception &e) {
            PLOG_ERROR << "failed to destroy ep '" << info.epName << "': " << e.what();
        }

        // either way, close the fd
        close(info.chrdevFd);
    }

    this->rpcChannels.clear();

    // iterate the directory and check if names are "rpmsgN"
    const std::regex kNameRegex{"rpmsg\\d+"};
    for(const auto &dent : std::filesystem::directory_iterator{"/dev/"}) {
        std::smatch m;
        if(!std::regex_match(dent.path().filename().native(), m, kNameRegex)) {
            continue;
        }

        // try to open the file; then destroy the endpoint
        try {
            PLOG_VERBOSE << "destroying leftover ep " << dent.path().native();

            this->destroyRpcEndpoint(dent.path());
            count++;
        } catch(const std::exception &e) {
            PLOG_ERROR << "failed to destroy ep '" << dent.path() << "': " << e.what();
        }
    }

    return count;
}

/**
 * @brief Destroy an RPC endpoint, given its path
 *
 * @param path Path to the rpmsg_chrdev device to destroy
 */
void Coprocessor::destroyRpcEndpoint(const std::filesystem::path &path) {
    int fd = open(path.native().c_str(), O_RDWR);
    if(fd == -1) {
        throw std::system_error(errno, std::generic_category(), "open rpmsg_chrdev");
    }

    this->destroyRpcEndpoint(fd);

    close(fd);
}

/**
 * @brief Destroy an RPC endpoint, given an open file descriptor
 *
 * This invokes the `RPMSG_DESTROY_EPT_IOCTL` ioctl which will remove this device.
 *
 * @param fd File descriptor to invoke the destroy ioctl on
 */
void Coprocessor::destroyRpcEndpoint(const int fd) {
    int err = ioctl(fd, RPMSG_DESTROY_EPT_IOCTL, nullptr);
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "RPMSG_DESTROY_EPT_IOCTL");
    }
}



/**
 * @brief Dump a packet to the debug output
 *
 * @param what Description of the packet
 * @param packet Packet data to hexdump
 */
void Coprocessor::EndpointHandler::DumpPacket(const std::string_view &what,
        std::span<const std::byte> packet) {
    std::stringstream str;
    for(size_t i = 0; i < packet.size(); i++) {
        str << fmt::format("{:02x} ", packet[i]);
        if(i && !(i % 16)) {
            str << std::endl;
        }
    }

    PLOG_DEBUG << what << ":" << std::endl << str.str();
}
