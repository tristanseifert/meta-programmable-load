#include <fmt/format.h>
#include <plog/Log.h>

#include "EventLoop.h"
#include "LedManager.h"

/**
 * @brief Register a LED driver
 *
 * All registered drivers will be invoked in sequence, until one handles an indicator set request.
 */
void LedManager::registerDriver(const std::shared_ptr<DriverInterface> &driver) {
    this->drivers.emplace_back(driver);
}

/**
 * @brief Set the brightness of an indicator
 */
void LedManager::setBrightness(const Indicator which, const double brightness) {
    for(const auto &ptr : this->drivers) {
        auto driver = ptr.lock();
        if(!driver) {
            continue;
        }

        if(driver->setIndicatorBrightness(which, brightness)) {
            return;
        }
    }

    PLOG_WARNING << fmt::format("failed to set indicator {}={}: no driver",
            static_cast<size_t>(which), brightness);
}

/**
 * @brief Set the color of an indicator
 */
void LedManager::setColor(const Indicator which, const Color &color) {
    for(const auto &ptr : this->drivers) {
        auto driver = ptr.lock();
        if(!driver) {
            continue;
        }

        if(driver->setIndicatorColor(which, color)) {
            return;
        }
    }

    const auto [cR, cG, cB] = color;
    PLOG_WARNING << fmt::format("failed to set indicator {}=({}, {}, {}): no driver",
            static_cast<size_t>(which), cR, cG, cB);
}
