#include <getopt.h>
#include <unistd.h>
#include <event2/event.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

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

    // parse command line
    // TODO: do this

    // perform initialization
    InitLog(logLevel, logSimple);

    // boot coprocessor
    Coprocessor cop;

    cop.loadFirmware(fwPath);
    cop.start();

    // set up communications
    // TODO: implement
    InitLibevent();

    // enter the event loop
    while(gRun) {
        // TODO: implement
        break;
    }

    // shut down the coprocessor
    cop.stop();

    // perform clean-up
    // TODO: implement

    return 0;
}
