/**
 * @file
 *
 * @brief Message format of RPC protocol to splash screen
 */
#ifndef SPLASH_REMOTELIB_RPC_H
#define SPLASH_REMOTELIB_RPC_H

#include <stdint.h>

/**
 * @brief Splash daemon message type
 */
typedef enum {
    kSplashRpcTypeNone,
    /// Set the percentage of progress completed
    kSplashRpcTypeSetProgress,
    /// Update the message displayed above the progress bar
    kSplashRpcTypeSetMessage,
    /// Set a version string
    kSplashRpcTypeSetVersion,
    /// Ask the splash screen to terminate
    kSplashRpcTypeTerminate,
} splash_rpc_type_t;

/**
 * @brief Message struct sent to splash screen daemon
 *
 * This is a fixed size struct which contains the messages we can send to the daemon. No regard is
 * paid to the endianness of values, since this is going to be local and not over the network.
 */
typedef struct {
    splash_rpc_type_t type;

    /**
     * @brief Message payload
     *
     * This union contains a s truct for each of the valid message types. Only one of these is
     * valid at a time, as decided by `type`.
     */
    union {
        /// New progress value
        double progress;
        /// Progress message (UTF-8, zero terminated)
        char message[256];
        /// Update one of the version strings
        struct {
            /// String slot
            uint8_t slot;
            /// Version string content
            char value[255];
        } version;
    };
} splash_rpc_message_t;

#endif
