#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <event2/event.h>

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
#include "DataStore.h"
#include "RpcServer.h"
#include "watchdog.h"
#include "version.h"

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

    PLOG_VERBOSE << "Logging initialized - confd " << kVersion << " (" << kVersionGitHash << ")";
}

/**
 * @brief Initialize libevent
 *
 * Configure the log callback for libevent to use our existing logging machinery.
 */
static void InitLibevent() {
    event_set_log_callback([](const auto severity, const auto msg) {
        switch(severity) {
            case EVENT_LOG_DEBUG:
                PLOG_DEBUG << msg;
                break;
            case EVENT_LOG_MSG:
                PLOG_INFO << msg;
                break;
            case EVENT_LOG_WARN:
                PLOG_WARNING << msg;
                break;
            default:
                PLOG_ERROR << msg;
                break;
        }
    });
}

/**
 * @brief Server's main loop
 *
 * Continually handle events on the RPC sockets (this is done with libevent behind the scenes)
 * until the run flag is cleared.
 *
 * This is a separate function so we run the destructor of the RPC server immediately to begin the
 * shutdown sequence, including closing the listening sockets and any remaining client connections
 * and their associated resources.
 */
static void MainLoop(const std::shared_ptr<DataStore> &db) {
    RpcServer server(db);

    PLOG_VERBOSE << "starting runloop";

    // start the watchdog here (it's kicked in the run loop)
    Watchdog::Start();

    // run until flag is cleared
    while(gRun) {
        server.run();
    }

    // stop the watchdog here (we'll no longer be kicking it)
    PLOG_INFO << "shutting down...";
    Watchdog::Stop();
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
    std::shared_ptr<DataStore> store;

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

    // do basic initialization and set up config
    InitLog(logLevel, logSimple);
    Watchdog::Init();

    try {
        PLOG_DEBUG << "Reading config: " << confPath;
        Config::Read(confPath);
        PLOG_DEBUG << "Finished reading config";
    } catch(const toml::parse_error &err) {
        PLOG_FATAL << "failed to parse config: " << err;
        return 2;
    } catch(const std::runtime_error &err) {
        PLOG_FATAL << "config invalid: " << err.what();
        return 2;
    }

    // open and initialize data store
    try {
        store = std::make_unique<DataStore>(Config::GetStoragePath());
    } catch(const std::exception &err) {
        PLOG_FATAL << "failed to initialize data store: " << err.what();
        return 1;
    }

    // perform server setup, then enter run loop
    try {
        InitLibevent();
        MainLoop(store);
    } catch(const std::exception &err) {
        PLOG_FATAL << "failed to start server: " << err.what();
        return 1;
    }

    // close data store
    store.reset();

    return 0;
}
