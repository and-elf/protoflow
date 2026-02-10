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
    
    std::string name() const override { return "rpc_service"; }

protected:
    void handle(service::Message&& msg) override;
    std::vector<service::Message> generate_outbound() override;

private:
    struct Connection {
        ConnectionId id;
        transport::tcp::tcp_client client;
        bool connected = false;
        std::vector<std::vector<std::byte>> pending_sends;
    };
    
    void handle_connect_request(const RpcConnectRequest& req);
    void handle_send_request(const RpcSendRequest& req);
    void handle_disconnect_request(const RpcDisconnectRequest& req);
    
    void poll_connection(Connection& conn);
    void try_send_pending(Connection& conn);
    void try_receive(Connection& conn);
    
    std::unordered_map<ConnectionId, std::unique_ptr<Connection>> connections_;
    std::vector<service::Message> outbound_;
};

} // namespace protoflow::rpc_service
