#ifndef RPC_LOADDCLIENT_H
#define RPC_LOADDCLIENT_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>

#include <load-common/Rpc/ClientBase.h>

namespace Rpc {
class LoaddClient: public PlCommon::Rpc::ClientBase {
    public:
        /**
         * @brief Data point from a measurement message
         */
        struct Measurement {
            double voltage{0};
            double current{0};
            double temperature{0};
        };
        /// Type for a callback invoked with new measurement data
        using MeasurementCallback = std::function<void(const Measurement &)>;

    public:
        LoaddClient(const std::filesystem::path &path) : ClientBase(path) {};

        uint32_t addMeasurementCallback(const MeasurementCallback &cb);
        bool removeMeasurementCallback(const uint32_t token);

    protected:
        void handleIncomingMessage(const PlCommon::Rpc::RpcHeader &header,
                const struct cbor_item_t *message) override final;

    private:
        void processMeasurement(const struct cbor_item_t *);

    private:
        /// Measurement callbacks (mapped by unique token)
        std::unordered_map<uint32_t, MeasurementCallback> measurementCallbacks;
        /// Next measurement callback token
        uint32_t nextMeasurementCallbackToken{0};
};
}

#endif
