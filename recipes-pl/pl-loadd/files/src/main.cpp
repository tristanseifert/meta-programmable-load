#include <getopt.h>
#include <unistd.h>
#include <event2/event.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <plog/Log.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/FuncMessageFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include "Coprocessor.h"
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
 * Entry point for config daemon
 *
 * Start off by parsing command line arguments, then boot the coprocessor and establish all
 * communications channels.
 */
int main(const int argc, char * const * argv) {
    //plog::Severity logLevel{plog::Severity::info};
    plog::Severity logLevel{plog::Severity::verbose};
    bool logSimple{false};
    std::filesystem::path fwPath{"/tmp/balls.elf"};

    std::unique_ptr<Coprocessor> cop;

    // parse command line
    // TODO: do this

    // perform initialization
    InitLog(logLevel, logSimple);

    try {
        // boot coprocessor
        cop = std::make_unique<Coprocessor>();

        cop->loadFirmware(fwPath);
        cop->start();

        // set up communications
        InitLibevent();

        /*
         * Insert a short wait before we try to enable the RPC interface. This is required because
         * the M4 firmware needs to do some setup during boot to start exposing the virtio rings
         * and then notify the host.
         *
         * If we don't have this wait, the rpmsg subsystem won't have initialized, and we'll fail
         * to open the control device.
         */
        std::this_thread::sleep_for (std::chrono::milliseconds(500));
        cop->initRpc();
    } catch(const std::exception &e) {
        PLOG_FATAL << "failed to start " << argv[0] << ": " << e.what();
        return 1;
    }

    // enter the event loop
    while(gRun) {
        // TODO: implement
        break;
    }

    // perform cleanup
    try {
        // shut down the coprocessor
        cop->stop();
        cop.reset();

        // perform other clean-up
    } catch(const std::exception &e) {
        PLOG_ERROR << "failed to shut down " << argv[0] << ": " << e.what();
        return 1;
    }

    return 0;
}
