#include "config.h"
#include "FbSurface.h"
#include "Drawer.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/kd.h>
#include <sys/utsname.h>

#include <atomic>
#include <iostream>
#include <sstream>

/// Whether the main loop of the splash shall run
std::atomic_bool gRun{true};

/// File descriptor for tty (to disable its gfx)
static int gTtyFd{-1};

/**
 * @brief Termination signal handler
 *
 * This just sets the flag to terminate.
 */
static void SignalHandler(int signum) {
    gRun = false;
}

/**
 * @brief Install signal handlers
 *
 * Install the termination handler for SIGINT (^C), SIGHUP, and SIGTERM.
 */
static void InstallSignalHandler() {
    struct sigaction newAction, oldAction;

    // all signals use the same handler
    newAction.sa_handler = SignalHandler;
    sigemptyset(&newAction.sa_mask);
    newAction.sa_flags = 0;

    // install them, unless said signal is ignored
    sigaction(SIGINT, NULL, &oldAction);
    if(oldAction.sa_handler != SIG_IGN) {
        sigaction(SIGINT, &newAction, NULL);
    }

    sigaction(SIGHUP, NULL, &oldAction);
    if(oldAction.sa_handler != SIG_IGN) {
        sigaction(SIGHUP, &newAction, NULL);
    }

    sigaction(SIGTERM, NULL, &oldAction);
    if(oldAction.sa_handler != SIG_IGN) {
        sigaction(SIGTERM, &newAction, NULL);
    }
}



/**
 * @brief Opens the TTY.
 */
static void OpenTty() {
    gTtyFd = open("/dev/tty0", O_RDWR);
}

/**
* @brief Disable TTY console on framebuffer
*/
static void DisableConsole() {
    if(gTtyFd == -1) {
        OpenTty();
    }

    if(ioctl(gTtyFd, KDSETMODE, KD_GRAPHICS) == -1) {
        throw std::system_error(errno, std::generic_category(), "KDSETMODE");
    }
}

/**
* @brief Enable TTY console on framebuffer
*/
static void EnableConsole() {
    if(gTtyFd == -1) {
        OpenTty();
    }

    if(ioctl(gTtyFd, KDSETMODE, KD_TEXT) == -1) {
        throw std::system_error(errno, std::generic_category(), "KDSETMODE");
    }
}



/**
 * @brief Update version string
 */
static void UpdateVersionString(Drawer &d) {
    int err;

    std::stringstream str;

    // get kernel version
    struct utsname unameInfo;
    err = uname(&unameInfo);

    if(!err) {
        str << "Kernel: " << unameInfo.sysname << " " << unameInfo.release << std::endl;
    }

    // update it
    d.setVersion(str.str());
}



/**
 * Entry point of boot splash
 *
 * This opens the framebuffer, maps it in memory, and then enters the main drawing/event loop.
 */
int main(const int argc, const char **argv) {
    int ret{-1};

    // install signal handler for graceful termination
    InstallSignalHandler();

    // open framebuffer, clear, and draw initial frame
    DisableConsole();

    FbSurface fb(Config::kFramebufferDevice);
    fb.clear(0, 0, 0);

    Drawer drawer(fb);
    UpdateVersionString(drawer);

    drawer.draw();

    // open listening socket (for events)

    // process messages
    while(gRun) {
        // TODO: poll on the eventfd/wait for signal
        pause();
    }

    // if we get here, we exited normally
    ret = 0;

    // clean up
    EnableConsole();

    if(gTtyFd > 0) {
            close(gTtyFd);
    }
    return ret;
}
