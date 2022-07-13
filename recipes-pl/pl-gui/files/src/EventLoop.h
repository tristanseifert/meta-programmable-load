#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <sys/signal.h>

#include <array>
#include <cstddef>

/**
 * @brief Main event loop
 *
 * This sets up the libevent-based main loop. By default, we'll have a signal handler installed (so
 * that the task can be terminated with Ctrl+C) as well as the watchdog handler, if the watchdog is
 * active; no other events are installed.
 *
 * Other components of the GUI task may add their event sources to the loop as needed.
 */
class EventLoop {
    public:
        EventLoop();
        ~EventLoop();

        void run();

        /**
         * @brief Get libevent main loop
         */
        inline auto getEvBase() {
            return this->evbase;
        }

    private:
        void initWatchdogEvent();
        void initSignalEvents();

        void handleTermination();

    private:
        /// signals to intercept
        constexpr static const std::array<int, 3> kEvents{{SIGINT, SIGTERM, SIGHUP}};
        /// termination signal events
        std::array<struct event *, 3> signalEvents;

        /// watchdog kicking timer event (if watchdog is active)
        struct event *watchdogEvent{nullptr};

        /// libevent main loop
        struct event_base *evbase{nullptr};
};

#endif
