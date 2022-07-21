#include <cstring>
#include <stdexcept>

#include <cbor.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "Utils/Cbor.h"
#include "Ft6336.h"

using namespace drivers::touch;

/**
 * @brief Initialize touch controller driver
 *
 * This parses the specified configuration data to set up the controller.
 *
 * @param busFd Opened file descriptor for the I²C bus
 * @param config Driver configuration payload (should be a map)
 */
Ft6336::Ft6336(const int busFd, const cbor_item_t *config) : busFd(busFd) {
    // validate the config entry as being a map and then read the config
    if(!cbor_isa_map(config)) {
        throw std::runtime_error("invalid config (expected map)");
    }

    this->readConfig(config);
    PLOG_DEBUG << fmt::format("Ft6336: addr ${:02x}, with {}", this->address,
            this->irqEnabled ? "interrupt" : "polling");

    if(!this->address) {
        throw std::runtime_error("failed to get device address");
    }
}

/**
 * @brief Parse the driver's configuration data
 *
 * This is a CBOR map with the following keys:
 *
 * - addr: Bus address for the touch controller
 * - irq: Whether the controller supports interrupts; set to `false` to use timer-driven polling.
 */
void Ft6336::readConfig(const cbor_item_t *inConfig) {
    auto entries = cbor_map_handle(inConfig);
    const auto numEntries = cbor_map_size(inConfig);

    for(size_t i = 0; i < numEntries; i++) {
        auto &pair = entries[i];

        // keys should be string
        if(!cbor_isa_string(pair.key) || !cbor_string_is_definite(pair.key)) {
            throw std::runtime_error("invalid key (expected definite string)");
        }

        const auto keyStr = reinterpret_cast<const char *>(cbor_string_handle(pair.key));
        const auto keyStrLen = cbor_string_length(pair.key);

        // process keys
        if(!strncmp("addr", keyStr, keyStrLen)) {
            if(!cbor_isa_uint(pair.value)) {
                throw std::runtime_error("invalid address (expected uint)");
            }

            this->address = Util::CborReadUint(pair.value);
        }
        else if(!strncmp("irq", keyStr, keyStrLen)) {
            // boolean flag?
            if(cbor_float_ctrl_is_ctrl(pair.value)) {
                this->irqEnabled = cbor_get_bool(pair.value);
            }
        }
    }
}
