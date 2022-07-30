#ifndef SHAREDSTATE_H
#define SHAREDSTATE_H

#include <memory>

namespace Rpc {
class PinballClient;
class LoaddClient;
}

/**
 * @brief Holder for globally shared state
 *
 * This class is a basic receptacle to hold common shared state used by all of the app; this is
 * mostly stuff like RPC clients.
 */
class SharedState {
    public:
        SharedState() = delete;

        /// Loadd rpc client
        static std::shared_ptr<Rpc::LoaddClient> gRpcLoadd;
        /// Pinball rpc client
        static std::shared_ptr<Rpc::PinballClient> gRpcPinball;
};

#endif
