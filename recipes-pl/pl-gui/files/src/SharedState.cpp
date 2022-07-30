#include "SharedState.h"

std::shared_ptr<Rpc::LoaddClient> SharedState::gRpcLoadd;
std::shared_ptr<Rpc::PinballClient> SharedState::gRpcPinball;
