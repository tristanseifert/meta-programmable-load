#ifndef CONNECTION_H
#define CONNECTION_H

#include <string_view>

/**
 * @brief RPC connection to confd
 *
 * This class encapsulates an RPC socket connection to confd. It holds all of the buffers we re-use
 * during the life of the connection as well.
 */
class Connection {
    constexpr static const std::string_view kDefaultSocketPath{"/var/run/confd/rpc.sock"};

    public:
        Connection(const std::string_view &socketPath = kDefaultSocketPath);
};

#endif
