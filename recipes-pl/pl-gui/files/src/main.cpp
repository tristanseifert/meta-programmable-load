#include <getopt.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <iostream>

#include <load-common/EventLoop.h>
#include <load-common/Watchdog.h>
#include <load-common/Logging.h>

#include "Framebuffer.h"
#include "version.h"
#include "Gui/IconManager.h"
#include "Gui/Renderer.h"
#include "Gui/VersionScreen.h"
#include "Rpc/PinballClient.h"
#include "Rpc/LoaddClient.h"

/// Whether the UI task shall continue running
std::atomic_bool gRun{true};

/**
 * @brief Entry point
 *
 * This ensures the drm framebuffer's set up, then jumps into our GUI main loop, and handles
 * cleaning up the drm stuff when we're done with all that.
 */
int main(const int argc, char * const * argv) {
    std::shared_ptr<PlCommon::EventLoop> ev;
    std::shared_ptr<Rpc::LoaddClient> rpc;
    std::shared_ptr<Rpc::PinballClient> pinballRpc;

    std::shared_ptr<Framebuffer> fb;
    std::shared_ptr<Gui::Renderer> gui;
    int logLevel;
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

        c = getopt_long_only(argc, argv, "", options, &index);

        // end of options
        if(c == -1) {
            break;
        }
        // long option (based on index)
        else if(!c) {
            if(index == 0) {
                logLevel = strtol(optarg, nullptr, 10);
            }
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
    PlCommon::InitLogging(logLevel, logSimple);
    PLOG_INFO << "Starting load-gui " << kVersion << " (" << kVersionGitHash << ")";

    PlCommon::Watchdog::Init();

    ev = std::make_shared<PlCommon::EventLoop>(true);
    ev->arm();

    // set up RPC to loadd
    PLOG_DEBUG << "initializing rpc";

    try {
        rpc = std::make_shared<Rpc::LoaddClient>(ev, loaddSocketPath);
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

        auto vers = std::make_shared<Gui::VersionScreen>(rpc);
        gui->setRootViewController(vers);

        pinballRpc->enableUiEvents(gui);
    } catch(const std::exception &e) {
        PLOG_FATAL << "failed to set up gui: " << e.what();
        return 1;
    }

    // run event loop
    PLOG_DEBUG << "entering main loop";
    PlCommon::Watchdog::Start();

    try {
        while(gRun) {
            ev->run();
        }
    } catch(const std::exception &e) {
        PLOG_FATAL << "exception in main loop: " << e.what();
    }

    PlCommon::Watchdog::Stop();

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
