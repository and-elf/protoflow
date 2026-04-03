#pragma once

#include <protoflow/rpc/protocol.hpp>
#include <protoflow/transport/tcp.hpp>
#include <protoflow/transport/unix.hpp>
#include <memory>
#include <string>
#include <optional>
#include <exception>

namespace protoflow::mainapp {
/// Factory function to create a server transport based on type and address
std::unique_ptr<protoflow::rpc::transport_interface> create_server_transport(
    const std::string& type,
    const std::string& address,
    std::optional<uint16_t> port
) {
    if (type == "tcp") {
        if (!port) throw std::invalid_argument("TCP transport requires a port");
        protoflow::transport::tcp::tcp_config cfg{};
        cfg.host = address;
        cfg.port = *port;
        return std::make_unique<protoflow::transport::tcp::tcp_server>(cfg);
    } else if (type == "unix") {
        protoflow::transport::unix::unix_config cfg{};
        cfg.socket_path = address;
        return std::make_unique<protoflow::transport::unix::unix_server>(cfg);
    } else {
        throw std::invalid_argument("Unsupported transport type: " + type);
    }

}

} // namespace protoflow::mainapp