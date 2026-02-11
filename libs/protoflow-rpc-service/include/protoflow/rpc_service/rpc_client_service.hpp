#pragma once

#include "messages.hpp"
#include <protoflow/service/service.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <memory>
#include <unordered_map>
#include <functional>

namespace protoflow::rpc_service {

/// RPC Client Service - manages multiple outbound RPC connections
/// Handles all network I/O via message passing
class RpcClientService : public service::Service {
public:
    /// Constructor
    explicit RpcClientService(std::unique_ptr<protoflow::rpc::transport_interface> transport);
    ~RpcClientService() override;

    // Service lifecycle
    void start() override;
    void stop() override;
    void poll() override;

protected:
    void handle(service::Message&& msg) override;
    std::vector<service::Message> generate_outbound() override;

    // Only one connection/transport
    std::unique_ptr<protoflow::rpc::transport_interface> transport_;
    std::vector<std::vector<std::byte>> pending_sends_;
    bool connected_ = false;
    ConnectionId connection_id_ = 1;
    std::vector<service::Message> outbound_;

    void handle_send_request(const RpcSendRequest& req);
    void handle_disconnect_request(const RpcDisconnectRequest& req);
    void poll_connection();
    void try_send_pending();
    void try_receive();
};

} // namespace protoflow::rpc_service
