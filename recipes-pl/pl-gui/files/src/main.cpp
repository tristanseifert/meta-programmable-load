#include <getopt.h>
#include <event2/event.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
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
#include "LoaddClient.h"
#include "Watchdog.h"
#include "version.h"
#include "Gui/IconManager.h"
#include "Gui/Renderer.h"
#include "Gui/HomeScreen.h"
#include "Rpc/PinballClient.h"

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
    std::shared_ptr<LoaddClient> rpc;
    std::shared_ptr<Rpc::PinballClient> pinballRpc;

    std::shared_ptr<Framebuffer> fb;
    std::shared_ptr<Gui::Renderer> gui;

    plog::Severity logLevel{plog::Severity::info};
    bool logSimple{false};

    // base path for icons
    std::filesystem::path iconBasePath{"/usr/share/pl-gui/icons"};
    // path to the loadd socket
    std::filesystem::path loaddSocketPath;
    /// path to the pinballd socket
    std::filesystem::path pinballdSocketPath;

    // parse command line
    int c;
    while(1) {
        int index{0};
        const static struct option options[] = {
            // log severity
            {"log-level",               optional_argument, 0, 0},
            // log style (simple = no timestamps, for systemd/syslog use)
            {"log-simple",              no_argument, 0, 0},
            // base path for icons
            {"iconbase",                required_argument, 0, 0},
            /// path to the loadd socket
            {"loadd-socket",            required_argument, 0, 0},
            /// path to the pinballd socket
            {"pinballd-socket",         required_argument, 0, 0},
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
            // icon base path
            else if(index == 2) {
                iconBasePath = optarg;
            }
            // loadd socket path
            else if(index == 3) {
                loaddSocketPath = optarg;
            }
            // pinballd socket path
            else if(index == 4) {
                pinballdSocketPath = optarg;
            }
        }
    }

    // initialization
    InitLog(logLevel, logSimple);
    InitLibevent();

    Watchdog::Init();

    ev = std::make_shared<EventLoop>();
    ev->arm();

    // set up RPC to loadd
    PLOG_DEBUG << "initializing rpc";

    try {
        rpc = std::make_shared<LoaddClient>(ev, loaddSocketPath);
        pinballRpc = std::make_shared<Rpc::PinballClient>(ev, pinballdSocketPath);
    } catch(const std::exception &e) {
        PLOG_FATAL << "failed to set up loadd rpc: " << e.what();
        return 1;
    }

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
        gui = std::make_shared<Gui::Renderer>(ev, fb);
        Gui::IconManager::SetBasePath(iconBasePath);

        auto home = std::make_shared<Gui::HomeScreen>(rpc);
        gui->setRootViewController(home);

        pinballRpc->enableUiEvents();
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
    pinballRpc->disableUiEvents();

    gui.reset();

    // clean up DRM resources
    PLOG_DEBUG << "cleaning up drm resources";
    fb.reset();

    // clean up everything else
    rpc.reset();
    pinballRpc.reset();

    ev.reset();
}
