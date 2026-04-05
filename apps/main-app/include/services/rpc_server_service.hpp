#pragma once

#include <protoflow/service.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <protoflow/rpc_service/messages.hpp>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <functional>

namespace protoflow::mainapp {

using ConnectionId = rpc_service::ConnectionId;

/// RPC Server Service configuration
struct RpcServerConfig {
    uint32_t client_timeout_seconds = 30;  ///< Remove clients inactive for this many seconds
};

/// RPC Server Service - manages a single server for inbound RPC connections
/// Handles all server I/O via message passing
/// NOTE: Only ONE instance should exist per system/installation
class RpcServerService : public service::Service {
public:
    /// Constructor with dependency injection
    /// @param server_transport Server transport for accepting connections
    explicit RpcServerService(
        std::unique_ptr<rpc::transport_interface> server_transport,
        const RpcServerConfig& config = RpcServerConfig{});
    ~RpcServerService() override;
    
    // Service lifecycle
    void start() override;
    void stop() override;
    void poll() override;

protected:
    void handle(service::Message&& msg) override;

public:
    struct ClientConnection {
        ConnectionId id;
        std::unique_ptr<rpc::transport_interface> transport;
        std::vector<std::vector<std::byte>> pending_sends;
        std::chrono::steady_clock::time_point last_activity_time;
        bool heartbeat_expected = false;
    };
    
    // Client timeout configuration (in seconds)
    uint32_t client_timeout_seconds_;
    
    void accept_new_connections();
    void poll_client(ClientConnection* client);
    void try_send_pending(ClientConnection* client);
    void try_receive(ClientConnection* client);
    void check_client_timeouts();
    
    void handle_server_send_request(const rpc_service::RpcSendRequest& req);
    void handle_server_disconnect_request(const rpc_service::RpcDisconnectRequest& req);
    
    std::unique_ptr<rpc::transport_interface> server_transport_;
    bool listening_{false};
    ConnectionId next_connection_id_{1};
    
    std::unordered_map<ConnectionId, std::unique_ptr<ClientConnection>> clients_;
};

} // namespace protoflow::mainapp
