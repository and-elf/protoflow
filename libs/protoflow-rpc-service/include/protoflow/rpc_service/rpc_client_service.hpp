#pragma once

#include "messages.hpp"
#include <protoflow/service/service.hpp>
#include <protoflow/rpc/rpc_base.hpp>
#include <memory>
#include <unordered_map>
#include <functional>

namespace protoflow::rpc_service {

/// RPC Client Service - manages multiple outbound RPC connections
/// Handles all network I/O via message passing
class RpcClientService : public service::Service {
public:
    /// Factory function type for creating transport instances
    /// Takes connection request and returns configured transport
    using TransportFactory = std::function<std::unique_ptr<rpc::transport_interface>(const RpcConnectRequest&)>;
    
    /// Constructor with dependency injection
    /// @param transport_factory Factory to create transport instances for outbound connections
    explicit RpcClientService(TransportFactory transport_factory);
    ~RpcClientService() override;
    
    // Service lifecycle
    void start() override;
    void stop() override;
    void poll() override;

protected:
    void handle(service::Message&& msg) override;
    std::vector<service::Message> generate_outbound() override;

private:
    struct Connection {
        ConnectionId id;
        std::unique_ptr<rpc::transport_interface> transport;
        std::vector<std::vector<std::byte>> pending_sends;
    };
    
    void handle_connect_request(const RpcConnectRequest& req);
    void handle_send_request(const RpcSendRequest& req);
    void handle_disconnect_request(const RpcDisconnectRequest& req);
    
    void poll_connection(Connection& conn);
    void try_send_pending(Connection& conn);
    void try_receive(Connection& conn);
    
    TransportFactory transport_factory_;
    std::unordered_map<ConnectionId, std::unique_ptr<Connection>> connections_;
    std::vector<service::Message> outbound_;
};

} // namespace protoflow::rpc_service
