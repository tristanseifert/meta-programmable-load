#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>
#include <system_error>

#include "confd.h"
#include "RpcConnection.h"
#include "version.h"

int confd_open(const char *socketPath) {
    auto realPath = socketPath ? socketPath : "/var/run/confd/rpc.sock";

    try {
        RpcConnection::Init(realPath);
    } catch(const std::system_error &e) {
        return -e.code().value();
    } catch(const std::exception &) {
        return -1;
    }
    return 0;
}

int confd_close() {
    RpcConnection::Deinit();
    return 0;
}

const char *confd_version_string() {
    return kVersion;
}
