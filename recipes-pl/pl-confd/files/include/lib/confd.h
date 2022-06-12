#ifndef LIBCONFD_H
#define LIBCONFD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status codes for confd library
 *
 * These positive status codes may be returned from various confd methods (namely, query and
 * setters) to indicate confd-specific issues, as compared to negative return codes which usually
 * correspond to system errors.
 */
enum confd_status {
    kConfdStatusSuccess                 = 0,
    /// The key exists, but its value is not of the specified type
    kConfdTypeMismatch                  = 1,
    /// Access to the key is denied
    kConfdAccessDenied                  = 2,
    /// Key does not exist
    kConfdNotFound                      = 3,
};


/**
 * @brief Establish connection to confd
 *
 * Set up the initial socket connection to confd. This must be invoked before any other calls in
 * this API. The connection will remain valid until it is explicitly closed with confd_close.
 *
 * @param socketPath Location of the UNIX domain socket confd is listening on, or `nullptr` to
 *        use the default
 *
 * @return 0 on success, or a negative error code.
 */
int confd_open(const char *socketPath);

/**
 * @brief Terminate confd connection
 *
 * Closes down any previously initialized connection.
 *
 * @remark It's valid to call this more than once; if the connection is already closed when this
 *         call is made, it does nothing.
 *
 * @return 0 on success, or a negative error code.
 */
int confd_close();

/**
 * @brief Read config key (string)
 *
 * Reads a configuration key, whose value is a string.
 *
 * @param key Config key to query
 * @param outStr Buffer to hold the string value
 * @param outStrLen Size of the output string buffer, in bytes
 *
 * @return Negative error code or one of the confd_status values.
 */
int confd_get_string(const char *key, char *outStr, const size_t outStrLen);

/**
 * @brief Read config key (integer)
 *
 * Read a configuration key as an integer value.
 *
 * @param key Config key to query
 * @param outValue Variable to receive the integer value
 *
 * @return Negative error code or one of the confd_status values.
 */
int confd_get_int(const char *key, int64_t *outValue);

#ifdef __cplusplus
}
#endif

#endif
