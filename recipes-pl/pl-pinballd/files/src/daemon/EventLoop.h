#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <sys/signal.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>

namespace Rpc {
class Server;
}

/**
 * @brief Main event loop
 *
 * This sets up the libevent-based main loop. By default, we'll have a signal handler installed (so
 * that the task can be terminated with Ctrl+C) as well as the watchdog handler, if the watchdog is
 * active; no other events are installed.
 *
 * Later on we'll add our RPC server event sources, and any event sources from drivers to this.
 */
class EventLoop: public std::enable_shared_from_this<EventLoop> {
    public:
        EventLoop(const std::filesystem::path &rpcSocketPath);
        ~EventLoop();

        /**
         * @brief Arm the event loop for execution
         *
         * This doesn't really do anything other than set it as the active event loop for the
         * calling thread.
         */
        void arm() {
            this->activate();
        }

        void run();

        /**
         * @brief Get libevent main loop
         */
        inline auto getEvBase() {
            return this->evbase;
        }

        /**
         * @brief Get the RPC server
         */
        constexpr inline auto &getRpcServer() {
            return this->rpc;
        }

        static std::shared_ptr<EventLoop> Current();

    private:
        void initWatchdogEvent();
        void initSignalEvents();

        void handleTermination();

        /**
         * @brief Mark this event loop as the calling thread's active loop
         */
        inline void activate() {
            gCurrentEventLoop = this->shared_from_this();
        }

    private:
        static thread_local std::weak_ptr<EventLoop> gCurrentEventLoop;

        /// signals to intercept
        constexpr static const std::array<int, 3> kEvents{{SIGINT, SIGTERM, SIGHUP}};
        /// termination signal events
        std::array<struct event *, 3> signalEvents;

        /// watchdog kicking timer event (if watchdog is active)
        struct event *watchdogEvent{nullptr};

        /// libevent main loop
        struct event_base *evbase{nullptr};

        /// Local RPC server
        std::shared_ptr<Rpc::Server> rpc;
};

#endif
