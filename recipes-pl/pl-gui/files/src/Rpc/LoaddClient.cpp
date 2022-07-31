#include <stdexcept>

#include <cbor.h>
#include <fmt/format.h>
#include <plog/Log.h>
#include <load-common/EventLoop.h>
#include <load-common/Rpc/Types.h>
#include <load-common/Utils/Cbor.h>

#include "LoaddClient.h"

using namespace Rpc;

/**
 * @brief RPC message endpoints
 */
enum RpcEndpoint: uint8_t {
    kRpcEndpointNoOp                    = 0x00,
    kRpcEndpointMeasurement             = 0x10,
};

/**
 * @brief Handle a received message
 */
void LoaddClient::handleIncomingMessage(const PlCommon::Rpc::RpcHeader &header,
        const struct cbor_item_t *message) {
    switch(header.endpoint) {
        case kRpcEndpointMeasurement:
            this->processMeasurement(message);
            break;

        default:
            PLOG_WARNING << fmt::format("unknown loadd rpc type ${:02x}", header.endpoint);
            break;
    }
}

/**
 * @brief Process a measurement packet
 *
 * Decode the measurements inside the packet and invoke all measurement packets. This packet type
 * is an unsolicited periodic message sent by the firmware.
 *
 * @param payload Buffer containing the CBOR encoded payload packet
 */
void LoaddClient::processMeasurement(const struct cbor_item_t *item) {
    Measurement meas{};

    if(!cbor_isa_map(item)) {
        throw std::invalid_argument("invalid payload: expected map");
    }

    // get voltage
    if(auto value = PlCommon::Util::CborMapGet(item, "v")) {
        if(!cbor_isa_float_ctrl(value)) {
            throw std::runtime_error("invalid voltage value (expected float)");
        }
        meas.voltage = cbor_float_get_float(value);
    }

    if(auto value = PlCommon::Util::CborMapGet(item, "i")) {
        if(!cbor_isa_float_ctrl(value)) {
            throw std::runtime_error("invalid current value (expected float)");
        }
        meas.current = cbor_float_get_float(value);
    }

    if(auto value = PlCommon::Util::CborMapGet(item, "t")) {
        if(!cbor_isa_float_ctrl(value)) {
            throw std::runtime_error("invalid temperature value (expected float)");
        }
        meas.temperature = cbor_float_get_float(value);
    }

    // invoke callbacks
    for(const auto &[token, cb] : this->measurementCallbacks) {
        cb(meas);
    }
}

/**
 * @brief Register a new measurement callback
 *
 * This is a callback that's invoked when we receive a measurement from the firmware.
 *
 * @return Measurement callback token, which can be used to remove the callback later
 */
uint32_t LoaddClient::addMeasurementCallback(const MeasurementCallback &cb) {
    uint32_t token;

    do {
        token = ++this->nextMeasurementCallbackToken;
    } while(!token || this->measurementCallbacks.contains(token));

    this->measurementCallbacks.emplace(token, cb);

    return token;
}

/**
 * @brief Remove a previously installed measurement callback
 *
 * @token A token value returned by registerMeasurementCallback
 *
 * @return Whether the callback was removed
 *
 * @seeAlso registerMeasurementCallback
 */
bool LoaddClient::removeMeasurementCallback(const uint32_t token) {
    return this->measurementCallbacks.erase(token);
}
