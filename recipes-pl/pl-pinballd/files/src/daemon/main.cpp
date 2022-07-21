#include <getopt.h>
#include <unistd.h>

#include <atomic>
#include <cctype>
#include <filesystem>
#include <memory>
#include <iostream>

#include <event2/event.h>
#include <fmt/format.h>
#include <plog/Log.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/FuncMessageFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include "Probulator.h"
#include "Watchdog.h"
#include "version.h"

/// Whether we shall continue to listen and process requests
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

    PLOG_VERBOSE << "Logging initialized - pinballd " << kVersion << " (" << kVersionGitHash << ")";

    // install libevent logging callback
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
 * Entry point
 *
 * Attempt to detect hardware (by probing EEPROM) and then initialize the appropriate drivers. Once
 * that's done, create the RPC listening socket.
 */
int main(const int argc, char * const * argv) {
    std::string socketPath;
    plog::Severity logLevel{plog::Severity::info};
    bool logSimple{false};

    std::shared_ptr<Probulator> probe;
    std::filesystem::path frontI2cBus;

    // parse command line
    int c;
    while(1) {
        int index{0};
        const static struct option options[] = {
            // listening socket path
            {"socket",                  required_argument, 0, 0},
            // log severity
            {"log-level",               optional_argument, 0, 0},
            // log style (simple = no timestamps, for systemd/syslog use)
            {"log-simple",              no_argument, 0, 0},
            // i2c bus on which the front panel lives
            {"front-i2c-bus",           required_argument, 0, 0},
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
                socketPath = optarg;
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
            /*
             * I2C bus: This is parsed as either a number (in which case it's appended to the I2C
             * bus device base path) or as a whole ass string. This determination is made by
             * checking if the first non-space character is a number.
             */
            else if(index == 3) {
                bool isPath{true};
                const char *argReadPtr = optarg;
                while(const auto ch = *argReadPtr) {
                    // skip spaces
                    if(isspace(ch)) {
                        argReadPtr++;
                        continue;
                    }

                    isPath = !isdigit(ch);
                    break;
                }

                if(isPath) {
                    frontI2cBus = optarg;
                } else {
                    frontI2cBus = fmt::format("/dev/i2c-{}", optarg);
                }
            }
        }
    }

    if(socketPath.empty()) {
        std::cerr << "you must specify a socket path (--socket)" << std::endl;
        return 1;
    }

    // basic initialize
    InitLog(logLevel, logSimple);
    Watchdog::Init();

    // set up and RPC main loop

    // probe hardware and init drivers
    try {
        probe = std::make_shared<Probulator>(frontI2cBus);

        probe->probe();
    } catch(const std::exception &e) {
        PLOG_ERROR << "Failed to probe hardware: " << e.what();
        return 1;
    }

    // clean up
    PLOG_INFO << "cleaning up";
    probe.reset();
}
