#pragma once

#include <protoflow/service.hpp>
#include <protoflow/rpc/rpc_base.hpp>
#include <protoflow/rpc_service/messages.hpp>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <functional>

namespace protoflow::mainapp {

using ConnectionId = rpc_service::ConnectionId;

/// RPC Server Service - manages a single server for inbound RPC connections
/// Handles all server I/O via message passing
/// NOTE: Only ONE instance should exist per system/installation
class RpcServerService : public service::Service {
public:
    /// Factory function type for creating client transport instances
    /// Used when accepting new connections
    using ClientTransportFactory = std::function<std::unique_ptr<rpc::transport_interface>()>;
    
    /// Factory function type for creating server transport instance
    using ServerTransportFactory = std::function<std::unique_ptr<rpc::transport_interface>()>;
    
    /// Constructor with dependency injection
    /// @param server_transport Factory to create the server transport
    /// @param client_transport_factory Factory to create client transports for accepted connections
    explicit RpcServerService(
        ServerTransportFactory server_transport,
        ClientTransportFactory client_transport_factory
    );
    ~RpcServerService() override;
    
    // Service lifecycle
    void start() override;
    void stop() override;
    void poll() override;

protected:
    void handle(service::Message&& msg) override;
    std::vector<service::Message> generate_outbound() override;

private:
    struct ClientConnection {
        ConnectionId id;
        std::unique_ptr<rpc::transport_interface> transport;
        std::vector<std::vector<std::byte>> pending_sends;
    };
    
    void accept_new_connections();
    void poll_client(ClientConnection& client);
    void try_send_pending(ClientConnection& client);
    void try_receive(ClientConnection& client);
    
    void handle_server_send_request(const rpc_service::RpcSendRequest& req);
    void handle_server_disconnect_request(const rpc_service::RpcDisconnectRequest& req);
    
    std::unique_ptr<rpc::transport_interface> server_transport_;
    ClientTransportFactory client_transport_factory_;
    bool listening_{false};
    ConnectionId next_connection_id_{1};
    
    std::unordered_map<ConnectionId, std::unique_ptr<ClientConnection>> clients_;
    std::vector<service::Message> outbound_;
};

} // namespace protoflow::mainapp
