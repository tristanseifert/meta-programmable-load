#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <string>

#include <toml++/toml.h>

#include "Config.h"
#include "watchdog.h"

/// Whether the server shall continue to listen and process requests
std::atomic_bool gRun{true};



/**
 * @brief Signal handler
 *
 * This handler is installed for all signals on which we quit; it sets an atomic flag, and signals
 * the main loop (if needed) so that it wakes up and we terminate.
 */
static void SignalHandler(int signum) {
    gRun = false;
}

/**
 * @brief Installs signal handlers
 *
 * Configures handlers for SIGINT, SIGHUP and SIGTERM. These in turn ensure that the server has
 * an orderly shutdown, including syncing the data store.
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
 * Entry point for config daemon
 *
 * We'll start the process initialization (opening the data store, initializing it if needed, then
 * opening the listening socket) before entering a loop to accept and process clients forever.
 */
int main(const int argc, char * const * argv) {
    std::string confPath;

    // parse command line
    int c;
    while(1) {
        int index{0};
        const static struct option options[] = {
            // config file
            {"config",                  required_argument, 0, 0},
            {nullptr,                   0, 0, 0},
        };

        c = getopt_long(argc, argv, "", options, &index);

        // end of options
        if(c == -1) {
            break;
        }
        // long option (based on index)
        else if(!c) {
            if(index == 0) {
                confPath = optarg;
            }
        }
    }

    if(confPath.empty()) {
        std::cerr << "you must specify a config file (--config)" << std::endl;
        return -1;
    }

    // early setup
    InstallSignalHandler();
    Watchdog::Init();

    try {
        Config::Read(confPath);
    } catch(const toml::parse_error &err) {
        std::cerr << "failed to parse config: " << err << std::endl;
        return -1;
    } catch(const std::runtime_error &err) {
        std::cerr << "config invalid: " << err.what() << std::endl;
        return -1;
    }

    // open and initialize data store
    // TODO: implement

    // set up server
    // TODO: implement

    // main server loop
    Watchdog::Start();

    while(gRun) {
        // TODO: stuff
        sleep(2);
        Watchdog::Kick();
    }

    Watchdog::Stop();

    // shut down server connections

    // close data store

    return 0;
}
