#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include <cbor.h>
#include <fmt/format.h>
#include <plog/Log.h>
#include <load-common/EventLoop.h>
#include <load-common/Rpc/Types.h>
#include <load-common/Utils/Cbor.h>
#include <shittygui/Event.h>
#include <shittygui/Screen.h>

#include "Gui/Renderer.h"
#include "PinballClient.h"

using namespace Rpc;

/**
 * @brief RPC message endpoints
 */
enum RpcEndpoint: uint8_t {
    kRpcEndpointNoOp                    = 0x00,
    kRpcEndpointBroadcastConfig         = 0x01,
    kRpcEndpointUiEvent                 = 0x02,
    kRpcEndpointIndicator               = 0x03,
};

/**
 * @brief Handle a received message
 */
void PinballClient::handleIncomingMessage(const PlCommon::Rpc::RpcHeader &header,
        const struct cbor_item_t *message) {
    switch(header.endpoint) {
        case kRpcEndpointUiEvent:
            this->processUiEvent(message);
            break;

        case kRpcEndpointNoOp:
            break;
        default:
            PLOG_WARNING << fmt::format("unknown pinballd rpc type ${:02x}", header.endpoint);
            break;
    }
}

/**
 * @brief Process a received user event broadcast
 *
 * Generate an event and inject it into the GUI subsystem as appropriate.
 */
void PinballClient::processUiEvent(const struct cbor_item_t *item) {
    if(!cbor_isa_map(item)) {
        throw std::invalid_argument("invalid payload: expected map");
    }

    /*
     * Figure out the type of event we're dealing with by iterating the map until we find the
     * `type` key.
     */
    enum class EventType {
        Unknown, Touch, Button, Encoder
    };

    EventType type{EventType::Unknown};

    auto typeKey = PlCommon::Util::CborMapGet(item, "type");
    if(!typeKey || !cbor_isa_string(typeKey)) {
        throw std::runtime_error("missing or invalid event type key");
    }

    const std::string_view value{reinterpret_cast<const char *>(cbor_string_handle(typeKey)),
        cbor_string_length(typeKey)};

    if(value == "touch") {
        type = EventType::Touch;
    } else if(value == "button") {
        type = EventType::Button;
    } else if(value == "encoder") {
        type = EventType::Encoder;
    } else {
        PLOG_WARNING << fmt::format("Unknown UI event type '{}'", value);
        return;
    }

    // invoke the appropriate handler
    switch(type) {
        case EventType::Touch:
            this->processUiTouchEvent(item);
            break;

        // shouldn't get here
        default:
            break;
    }
}

/**
 * @brief Process a touch event
 *
 * This is a map which has a `touchData` key, which in turn is another map indexed by the touch
 * index. Each index may either be set to null, or another map which contains information about
 * a particular touch.
 */
void PinballClient::processUiTouchEvent(const struct cbor_item_t *root) {
    // get touch data
    auto touchData = PlCommon::Util::CborMapGet(root, "touchData");
    if(!touchData) {
        throw std::runtime_error("invalid touch event (missing touchData payload)");
    }

    // iterate over all touches we've event data for
    const auto numTouches = cbor_map_size(touchData);
    auto touches = cbor_map_handle(touchData);

    for(size_t j = 0; j < numTouches; j++) {
        auto &touchPair = touches[j];

        if(!cbor_isa_uint(touchPair.key)) {
            throw std::runtime_error("invalid touch key");
        } else if(!cbor_isa_map(touchPair.value) && !cbor_isa_float_ctrl(touchPair.value)) {
            throw std::runtime_error("invalid touch value (expected map or null)");
        }

        const auto touchId = PlCommon::Util::CborReadUint(touchPair.key);

        // currently, ignore the second+ touches
        if(touchId != 0) {
            continue;
        }

        // emit the appropriate touch event
        if(cbor_is_null(touchPair.value)) {
            this->emitTouchUp();
        } else {
            uint16_t posX{0}, posY{0};

            // get the position of the touch
            auto posArray = PlCommon::Util::CborMapGet(touchPair.value, "position");
            if(!posArray || !cbor_isa_array(posArray)) {
                throw std::runtime_error("invalid touch position (expected array)");
            }

            posX = PlCommon::Util::CborReadUint(cbor_array_get(posArray, 0));
            posY = PlCommon::Util::CborReadUint(cbor_array_get(posArray, 1));

            // emit an event
            this->emitTouchDown(posX, posY);
        }
    }
}

/**
 * @brief Send a touch event to the GUI layer
 */
void PinballClient::emitTouchEvent(const int16_t x, const int16_t y, const bool isDown) {
    // get the GUI instance
    auto gui = this->gui.lock();
    if(!gui) {
        PLOG_WARNING << "GUI went away, can't send touch event!";
        return;
    }

    gui->getScreen()->queueEvent(shittygui::event::Touch({x, y}, isDown));

    if(kLogEvents) {
        PLOG_VERBOSE << fmt::format("Touch event ({}, {}) {}", x, y, isDown ? "down" : "up");
    }
}


/**
 * @brief Update the remote on what types of broadcast packets we wish to receive
 *
 * @param mask Logical OR of PinballBroadcastType
 */
void PinballClient::setDesiredBroadcasts(const PinballBroadcastType mask) {
    // set up the CBOR map
    auto root = cbor_new_definite_map(3);

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("touch")),
        .value = cbor_move(cbor_build_bool(mask & PinballBroadcastType::TouchEvent))
    });

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("button")),
        .value = cbor_move(cbor_build_bool(mask & PinballBroadcastType::ButtonEvent))
    });

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("encoder")),
        .value = cbor_move(cbor_build_bool(mask & PinballBroadcastType::EncoderEvent))
    });

    // encode it and send it as a packet
    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);
    cbor_decref(&root);

    try {
        this->sendPacket(kRpcEndpointBroadcastConfig,
                {reinterpret_cast<std::byte *>(rootBuf), serializedBytes});
        free(rootBuf);
    } catch(const std::exception &e) {
        free(rootBuf);
        throw;
    }
}

/**
 * @brief Update the state of one or more indicators
 *
 * @param changes Block of memory containing one or more indicator change requests
 */
void PinballClient::setIndicatorState(std::span<const IndicatorChange> changes) {
    // mapping of indicator -> key name
    static const std::unordered_map<Indicator, std::string_view> kIndicatorNames{{
        {Indicator::Status,             "status"},
        {Indicator::Trigger,            "trigger"},
        {Indicator::Overheat,           "overheat"},
        {Indicator::Overcurrent,        "overcurrent"},
        {Indicator::Error,              "error"},
        {Indicator::BtnModeCc,          "modeCc"},
        {Indicator::BtnModeCv,          "modeCv"},
        {Indicator::BtnModeCw,          "modeCw"},
        {Indicator::BtnModeExt,         "modeExt"},
        {Indicator::BtnLoadOn,          "loadOn"},
        {Indicator::BtnMenu,            "menu"},
    }};

    // validate args
    if(changes.empty()) {
        // honestly, what kind of idiot would make this call?
        return;
    }

    // set up the encoder
    auto root = cbor_new_definite_map(changes.size());

    // encode each change
    for(const auto &change : changes) {
        const auto &[indicator, value] = change;

        // create the payload for this key based on the value
        auto payload = std::visit([&](auto&& arg) -> cbor_item_t * {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::is_same_v<T, double>) {
                return cbor_build_float8(arg);
            }
            else if constexpr(std::is_same_v<T, IndicatorColor>) {
                const auto [cR, cG, cB] = arg;

                auto array = cbor_new_definite_array(3);
                cbor_array_push(array, cbor_build_float4(cR));
                cbor_array_push(array, cbor_build_float4(cG));
                cbor_array_push(array, cbor_build_float4(cB));
                return array;
            }
            else if constexpr(std::is_same_v<T, bool>) {
                return cbor_build_bool(arg);
            }
        }, value);

        // add it to the map
        cbor_map_add(root, (struct cbor_pair) {
            .key = cbor_move(cbor_build_string(kIndicatorNames.at(indicator).data())),
            .value = cbor_move(payload)
        });
    }

    // send the packet
    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);
    cbor_decref(&root);

    try {
        this->sendPacket(kRpcEndpointIndicator,
                {reinterpret_cast<std::byte *>(rootBuf), serializedBytes});
        free(rootBuf);
    } catch(const std::exception &e) {
        free(rootBuf);
        throw;
    }
}
