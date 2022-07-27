#ifndef PLCOMMON_WATCHDOG_H
#define PLCOMMON_WATCHDOG_H

#include <chrono>

namespace PlCommon {
/**
 * @brief Interface to system process supervidsor watchdog
 *
 * This provides an interface to the process supervisor (systemd) watchdog facility, where we can
 * periodically sent an "alive" message. If this message is skipped, we'll get restarted. This is
 * used to recover from hangs or other unusual situations.
 */
class Watchdog {
    public:
        static void Init();
        static void Start();
        static void Stop();
        static void Kick();

        /// Is the watchdog enabled?
        static const bool IsActive() {
            return gIsActive;
        }
        /// Return the watchdog interval
        static const auto GetInterval() {
            return gInterval;
        }

    private:
        /// Whether the watchdog is activated
        static bool gIsActive;
        /// Interval at which the watchdog should be kicked
        static std::chrono::microseconds gInterval;
};
}

#endif
