#include <getopt.h>
#include <event2/event.h>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <iostream>

#include <plog/Log.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/FuncMessageFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include "EventLoop.h"
#include "Framebuffer.h"
#include "Gui.h"
#include "Watchdog.h"
#include "version.h"

/// Whether the UI task shall continue running
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
    // figure out if the console is a tty
    const bool isTty = (isatty(fileno(stdout)) == 1);

    // set up the logger
    if(simple) {
        if(isTty) {
            static plog::ColorConsoleAppender<plog::FuncMessageFormatter> ttyAppender;
            plog::init(level, &ttyAppender);
        } else {
            static plog::ConsoleAppender<plog::FuncMessageFormatter> ttyAppender;
            plog::init(level, &ttyAppender);
        }
    } else {
        if(isTty) {
            static plog::ColorConsoleAppender<plog::TxtFormatter> ttyAppender;
            plog::init(level, &ttyAppender);
        } else {
            static plog::ConsoleAppender<plog::TxtFormatter> ttyAppender;
            plog::init(level, &ttyAppender);
        }
    }
    PLOG_INFO << "Starting load-gui " << kVersion << " (" << kVersionGitHash << ")";
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
 * @brief Entry point
 *
 * This ensures the drm framebuffer's set up, then jumps into our GUI main loop, and handles
 * cleaning up the drm stuff when we're done with all that.
 */
int main(const int argc, char * const * argv) {
    std::shared_ptr<EventLoop> ev;
    std::shared_ptr<Framebuffer> fb;
    std::shared_ptr<Gui> gui;

    plog::Severity logLevel{plog::Severity::info};
    bool logSimple{false};

    // parse command line
    int c;
    while(1) {
        int index{0};
        const static struct option options[] = {
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
            // log verbosity (centered around warning level)
            if(index == 0) {
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
            else if(index == 1) {
                logSimple = true;
            }
        }
    }

    // initialization
    InitLog(logLevel, logSimple);
    InitLibevent();

    Watchdog::Init();

    ev = std::make_shared<EventLoop>();

    // set up RPC to loadd
    // TODO: implement this

    // set up drm (framebuffer)
    PLOG_DEBUG << "initializing drm";

    try {
        fb = std::make_shared<Framebuffer>(ev, "/dev/dri/card0");
    } catch(const std::exception &e) {
        PLOG_FATAL << "failed to set up drm: " << e.what();
        return 1;
    }

    // initialize GUI
    PLOG_DEBUG << "initializing gui";

    try {
        gui = std::make_shared<Gui>(ev, fb);
    } catch(const std::exception &e) {
        PLOG_FATAL << "failed to set up gui: " << e.what();
        return 1;
    }

    // run event loop
    PLOG_DEBUG << "entering main loop";

    try {
        while(gRun) {
            ev->run();
        }
    } catch(const std::exception &e) {
        PLOG_FATAL << "exception in main loop: " << e.what();
    }

    // clean up GUI
    gui.reset();

    // clean up DRM resources
    PLOG_DEBUG << "cleaning up drm resources";
    fb.reset();

    // clean up everything else
    ev.reset();
}
