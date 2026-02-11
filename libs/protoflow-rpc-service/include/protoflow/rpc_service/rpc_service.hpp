#pragma once

#include "messages.hpp"
#include <protoflow/service/service.hpp>
#include <protoflow/transport/tcp.hpp>
#include <memory>
#include <unordered_map>

namespace protoflow::rpc_service {

/// RPC Service - manages multiple RPC connections
/// Handles all network I/O via message passing
class RpcService : public service::Service {
public:
    RpcService();
    ~RpcService() override;
    
    // Service lifecycle
    void start() override;
    void stop() override;
    void poll() override;

protected:
    void handle(service::Message&& msg) override;
    std::vector<service::Message> generate_outbound() override;

// (struct Connection removed; use the one from rpc_client_service.hpp)
};

} // namespace protoflow::rpc_service
