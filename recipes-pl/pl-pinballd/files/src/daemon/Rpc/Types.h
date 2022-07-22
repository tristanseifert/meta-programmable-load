#ifndef RPC_TYPES_H
#define RPC_TYPES_H

namespace Rpc {
/// Types of broadcastable packets
enum class BroadcastType {
    TouchEvent,
    ButtonEvent,
    EncoderEvent,
};
}

#endif
