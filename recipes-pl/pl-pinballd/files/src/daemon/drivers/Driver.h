#ifndef DRIVERS_DRIVER_H
#define DRIVERS_DRIVER_H

class Probulator;

/**
 * @brief Base class for drivers
 *
 * This currently doesn't do anything other than serve as a convenient base class to use to hold
 * references to the drivers to prevent them from being deallocated while still in use.
 */
class DriverBase {
    public:
        virtual ~DriverBase() = default;

        /**
         * @brief The driver has just been registered
         *
         * Called right after the driver's registered to a probulator instance.
         *
         * @param probulator Instance the driver was registered to
         */
        virtual void driverDidRegister(Probulator *probulator) {
            // default implementation: do nothing
        }
};

#endif
