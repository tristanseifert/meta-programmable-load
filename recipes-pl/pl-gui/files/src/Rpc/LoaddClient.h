#ifndef RPC_LOADDCLIENT_H
#define RPC_LOADDCLIENT_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace PlCommon {
class EventLoop;
}

namespace Rpc {
class LoaddClient {
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
        LoaddClient(const std::shared_ptr<PlCommon::EventLoop> &ev,
                const std::filesystem::path &rpcSocketPath);
        ~LoaddClient();

        uint32_t addMeasurementCallback(const MeasurementCallback &cb);
        bool removeMeasurementCallback(const uint32_t token);

    private:
        int connectToLoadd();

        void handleLoaddRead(struct bufferevent *);
        void handleLoaddEvent(struct bufferevent *, const uintptr_t);

        void processMeasurement(std::span<const std::byte> payload);

    private:
        /// Event loop onto which our events are connected
        std::weak_ptr<PlCommon::EventLoop> ev;

        /// Path of the RPC socket
        std::filesystem::path socketPath;
        /// loadd remote socket
        int fd{-1};
        /// Buffer event wrapping the loadd socket
        struct bufferevent *bev{nullptr};

        /// Packet receive buffer
        std::vector<std::byte> rxBuf;

        /// Measurement callbacks (mapped by unique token)
        std::unordered_map<uint32_t, MeasurementCallback> measurementCallbacks;
        /// Next measurement callback token
        uint32_t nextMeasurementCallbackToken{0};
};
}

#endif
