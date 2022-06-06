/**
 * @brief Boot splash remote control library
 *
 * This library exposes an RPC interface to communicate with the boot splash daemon. Applications
 * can use it to 
 */
#ifndef SPLASH_REMOTELIB_H
#define SPLASH_REMOTELIB_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize connection to splash daemon
 *
 * This establishes a network connection with the splash deamon. This call must be made before any
 * other splash daemon remote calls.
 *
 * @return 0 on success, or a negative error code
 */
int splash_connect();

/**
 * @brief Close the splash daemon connection
 *
 * Tears down a previously opened splash connection.
 *
 * @return 0 on success, or a negative error code
 */
int splash_disconnect();

/**
 * @brief Update the state of the boot progress bar
 *
 * Update the percentage complete of the progress bar on the splash screen.
 *
 * @param percent A value [0, 1] indicating the relative progress of bootup.
 *
 * @return 0 on success, or a negative error code
 */
int splash_update_progress(const double percent);

/**
 * @brief Update the boot progress text
 *
 * Change the text displayed above the progress bar. Specify an empty string to hide it.
 *
 * @param str String to display above progress bar
 *
 * @return 0 on success, or a negative error code
 */
int splash_update_message(const char *str);

/**
 * @brief Request the splash daemon exits.
 *
 * This will cause the splash daemon to shut down and relinquish control over the framebuffer.
 */
int splash_request_exit();

#ifdef __cplusplus
}
#endif

#endif
