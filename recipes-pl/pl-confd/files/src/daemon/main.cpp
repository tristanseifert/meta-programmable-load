#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <string>

#include <plog/Log.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/FuncMessageFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <toml++/toml.h>

#include "Config.h"
#include "RpcServer.h"
#include "watchdog.h"

/// Whether the server shall continue to listen and process requests
std::atomic_bool gRun{true};


/**
 * @brief Initialize logging
 *
 * Sets up the plog logging framework. We redirect all log output to stderr, under the assumption
 * that we'll be running under some sort of supervisor that handles capturing and storing
 * these messages.
 *
 * @param level Minimum log level to output
 * @param simple Whether the simple message output format (no timestamps) is used
 */
static void InitLog(const plog::Severity level, const bool simple) {
    if(simple) {
        static plog::ConsoleAppender<plog::FuncMessageFormatter> ttyAppender;
        plog::init(level, &ttyAppender);
    } else {
        static plog::ConsoleAppender<plog::TxtFormatter> ttyAppender;
        plog::init(level, &ttyAppender);
    }

    PLOG_VERBOSE << "Logging initialized";
}

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
    plog::Severity logLevel{plog::Severity::info};
    bool logSimple{false};

    // parse command line
    int c;
    while(1) {
        int index{0};
        const static struct option options[] = {
            // config file
            {"config",                  required_argument, 0, 0},
            // log severity
            {"log-level",               optional_argument, 0, 0},
            // log style (simple = no timestamps, for systemd/syslog use)
            {"log-simple",              no_argument, 0, 0},
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
            // log verbosity (centered around warning level)
            else if(index == 1) {
                const auto level = strtol(optarg, nullptr, 10);

                switch(level) {
                    case -3:
                        logLevel = plog::Severity::fatal;
                        break;
                    case -2:
                        logLevel = plog::Severity::error;
                        break;
                    case -1:
                        logLevel = plog::Severity::warning;
                        break;
                    case 0:
                        logLevel = plog::Severity::info;
                        break;
                    case 1:
                        logLevel = plog::Severity::debug;
                        break;
                    case 2:
                        logLevel = plog::Severity::verbose;
                        break;

                    default:
                        std::cerr << "invalid log level: must be [-3, 2]" << std::endl;
                        return -1;
                }
            }
            // use simple log format
            else if(index == 2) {
                logSimple = true;
            }
        }
    }

    if(confPath.empty()) {
        std::cerr << "you must specify a config file (--config)" << std::endl;
        return -1;
    }

    // early setup
    InstallSignalHandler();
    InitLog(logLevel, logSimple);

    Watchdog::Init();

    try {
        PLOG_DEBUG << "Reading config: " << confPath;
        Config::Read(confPath);
        PLOG_DEBUG << "Finished reading config";
    } catch(const toml::parse_error &err) {
        PLOG_FATAL << "failed to parse config: " << err;
        return -1;
    } catch(const std::runtime_error &err) {
        PLOG_FATAL << "config invalid: " << err.what();
        return -1;
    }

    // open and initialize data store
    // TODO: implement

    // set up server
    RpcServer server;

    // main server loop
    Watchdog::Start();

    PLOG_VERBOSE << "starting runloop";
    while(gRun) {
        server.run();

        sleep(2);
        Watchdog::Kick();
    }
    PLOG_INFO << "shutting down...";

    Watchdog::Stop();

    // shut down server connections

    // close data store

    return 0;
}
