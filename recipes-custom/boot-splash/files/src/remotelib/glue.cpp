/**
 * @file
 *
 * @brief Remote control glue
 *
 * Provides the C glue functions to RPC interface.
 */

#include "remote.h"
#include "rpc.h"
#include "config.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

/// Socket to the RPC interface of the daemon
static int gSocket{-1};

/**
 * @brief Submit the message over the RPC socket.
 */
int SubmitMessage(const splash_rpc_message_t &msg) {
    int err = write(gSocket, &msg, sizeof(msg));

    if(err == -1) {
        return -errno;
    } else if(err != sizeof(msg)) {
        // TODO: handle insufficient write
        return -1;
    }

    return 0;
}

extern "C" {
/*
 * Open the domain socket on which the splash deaemon is listening.
 */
int splash_connect() {
    int err;

    // create the socket and associated address
    gSocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(gSocket == -1) {
        return -errno;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    *addr.sun_path = '\0';
    strncpy(addr.sun_path + 1, Config::kControlSocketPath.data(), sizeof(addr.sun_path) - 2);

    // dial it
    err = connect(gSocket, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if(err == -1) {
        return -errno;
    }

    // at this stage, we're good
    return 0;
}

/*
 * Close the previously opened connection by deallocating the handler.
 */
int splash_disconnect() {
    close(gSocket);
    gSocket = -1;

    return 0;
}

/*
 * Send an "update progress" message
 */
int splash_update_progress(const double percent) {
    int err;

    // prepare message
    splash_rpc_message_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = kSplashRpcTypeSetProgress;
    msg.progress = percent;

    // send it
    return SubmitMessage(msg);
}

/*
 * Send an "update progress string" message
 */
int splash_update_message(const char *str) {
    int err;

    // prepare message
    splash_rpc_message_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = kSplashRpcTypeSetMessage;
    strncpy(msg.message, str, sizeof(msg.message) - 1);

    // send it
    return SubmitMessage(msg);

}

/*
 * Send a "terminate pls" message
 */
int splash_request_exit() {
    // prepare message
    splash_rpc_message_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = kSplashRpcTypeTerminate;

    // send it
    return SubmitMessage(msg);
}
}
