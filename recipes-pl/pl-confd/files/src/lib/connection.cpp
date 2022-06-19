#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>
#include <system_error>

#include "confd.h"

/// Connected socket to confd
int gSocket{-1};
/// tag for the next outgoing message
uint8_t gNextTag{0};

/**
 * @brief Create and bind socket
 */
static void CreateSocket(const char *path) {
    int err;

    // create the socket
    gSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if(gSocket == -1) {
        throw std::system_error(errno, std::generic_category(), "create rpc socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    // dial it
    err = connect(gSocket, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "dial rpc socket");
    }
}



int confd_open(const char *socketPath) {
    // TODO: prevent against repeated opening?
    auto realPath = socketPath ? socketPath : "/var/run/confd/rpc.sock";

    try {
        CreateSocket(realPath);
    } catch(const std::system_error &e) {
        return -e.code().value();
    } catch(const std::exception &) {
        return -1;
    }
    return 0;
}

int confd_close() {
    // socket is already closed
    if(gSocket == -1) {
        return 0;
    }

    // ignore errors from close 
    close(gSocket);
    gSocket = -1;

    return 0;
}
