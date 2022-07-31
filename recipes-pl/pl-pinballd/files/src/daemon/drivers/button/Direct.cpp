#include <cbor.h>
#include <event2/event.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "EventLoop.h"
#include "RpcTypes.h"
#include "Rpc/Server.h"
#include "Utils/Cbor.h"

#include "drivers/gpio/GpioChip.h"
#include "Direct.h"

using namespace drivers::button;

/**
 * @brief Set up the polling timer
 */
void Direct::initPollingTimer() {
    this->pollingTimer = event_new(EventLoop::Current()->getEvBase(), -1, EV_PERSIST,
            [](auto, auto, auto ctx) {
        reinterpret_cast<Direct *>(ctx)->updateButtonState();
    }, this);
    if(!this->pollingTimer) {
        throw std::runtime_error("failed to allocate polling timer");
    }

    struct timeval tv{
        .tv_sec  = static_cast<time_t>(kPollInterval / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(kPollInterval % 1'000'000U),
    };

    evtimer_add(this->pollingTimer, &tv);
}

/**
 * @brief Clean up the polling timer
 */
void Direct::deallocPollingTimer() {
    if(this->pollingTimer) {
        event_free(this->pollingTimer);
    }
}

/**
 * @brief Initialize the GPIO
 *
 * This is currently hardcoded to configure the first 7 IOs on port 0 as inputs for buttons. These
 * inputs are active high.
 */
void Direct::initGpio() {
    this->buttonBits = 0b0111'1111U;
    this->buttonPolarity = 0b0111'1111U;

    for(size_t i = 0; i < this->buttonBits.size(); i++) {
        if(!this->buttonBits.test(i)) {
            continue;
        }

        this->gpio->configurePin(i, drivers::gpio::GpioChip::PinMode::Input);
    }

    // XXX: insert the fixed button map
    this->buttonMap = {{
        {0, Button::ModeCc},
        {1, Button::LoadOn},
        {2, Button::Select},
        {3, Button::ModeCw},
        {4, Button::ModeCv},
        {5, Button::ModeExt},
        {6, Button::Menu},
    }};
}

/**
 * @brief Read out the state of the IO expander
 */
void Direct::updateButtonState() {
    std::bitset<32> current;
    std::unordered_map<Button, bool> changes;

    // read current state
    current = this->gpio->getPinState() & this->buttonBits.to_ulong();
    if(current == this->buttonLastState) {
        return;
    }

    for(size_t i = 0; i < this->buttonBits.size(); i++) {
        // ensure this button is enabled _and_ changed
        if(!this->buttonBits.test(i)) {
            continue;
        }
        else if(this->buttonLastState.test(i) == current.test(i)) {
            continue;
        }

        // get corresponding button type
        const auto btnType = this->buttonMap.at(i);
        bool state{false};

        if(this->buttonPolarity.test(i)) { // active high
            state = current.test(i);
        } else { // active low
            state = !current.test(i);
        }

        changes.emplace(btnType, state);

        if(kLogChanges) {
            PLOG_VERBOSE << fmt::format("Button ${:02x} = {}", static_cast<uintptr_t>(btnType), state);
        }
    }

    // secrete a single button change event
    this->sendUpdate(changes);

    // update the state for next time
    this->buttonLastState = current;
}

/**
 * @brief Broadcast the button state changes
 *
 * This produces a CBOR map, with two keys: first, a string `type`=`button` and then a map with
 * string keys for each button, whose value is a boolean indicating its current state.
 */
void Direct::sendUpdate(const std::unordered_map<Button, bool> &changes) {
    // validate inputs
    if(changes.empty()) {
        throw std::runtime_error("no button updates to send!");
    }

    // set up the root of the message
    auto root = cbor_new_definite_map(2);
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("type")),
        .value = cbor_move(cbor_build_string("button"))
    });

    auto changesMap = cbor_new_definite_map(changes.size());

    // encode the changes
    for(const auto [btn, state] : changes) {
        const auto &name = kButtonNames.at(btn);

        cbor_map_add(changesMap, (struct cbor_pair) {
            .key = cbor_move(cbor_build_string(name.data())),
            .value = cbor_move(cbor_build_bool(state))
        });
    }

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("buttonData")),
        .value = cbor_move(changesMap)
    });

    // serialize the CBOR structure
    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);
    cbor_decref(&root);

    // now broadcast this packet
    try {
        EventLoop::Current()->getRpcServer()->broadcastRaw(Rpc::BroadcastType::ButtonEvent,
                kRpcEndpointUiEvent, {reinterpret_cast<std::byte *>(rootBuf), serializedBytes});
        free(rootBuf);
    } catch(const std::exception &) {
        free(rootBuf);
        throw;
    }
}
