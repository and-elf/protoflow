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
    // Adapter implementations that present a rpc::transport_interface
    // backed by tcp/unix server+clients.
    struct TcpClientAdapter : public protoflow::rpc::transport_interface {
        protoflow::transport::tcp::tcp_client client;
        explicit TcpClientAdapter(protoflow::transport::tcp::tcp_client c) noexcept : client(std::move(c)) {}

        bool send(std::span<const std::byte> data) override {
            auto res = client.send(data);
            return res && *res > 0;
        }

        std::expected<std::vector<std::byte>, std::string> receive(size_t max_size) override {
            auto r = client.receive(max_size);
            if (r) return *r;
            return std::unexpected(r.error().to_string());
        }

        void close() override { client.disconnect(); }
        bool is_connected() const override { return client.is_connected(); }
    };

    struct TcpServerAdapter : public protoflow::rpc::transport_interface {
        protoflow::transport::tcp::tcp_server server;
        explicit TcpServerAdapter(const protoflow::transport::tcp::tcp_config& cfg)
            : server(cfg) {}

        bool send(std::span<const std::byte>) override { return false; }

        std::expected<std::vector<std::byte>, std::string> receive(size_t) override {
            return std::unexpected(std::string("receive() not supported on server transport"));
        }

        void close() override { server.stop(); }
        bool is_connected() const override { return server.is_listening(); }

        std::unique_ptr<protoflow::rpc::transport_interface> accept() override {
            auto r = server.accept();
            if (!r) return nullptr;
            return std::make_unique<TcpClientAdapter>(std::move(*r));
        }
    };

    struct UnixClientAdapter : public protoflow::rpc::transport_interface {
        protoflow::transport::unix::unix_client client;
        explicit UnixClientAdapter(protoflow::transport::unix::unix_client c) noexcept : client(std::move(c)) {}

        bool send(std::span<const std::byte> data) override {
            auto res = client.send(data);
            return res && *res > 0;
        }

        std::expected<std::vector<std::byte>, std::string> receive(size_t max_size) override {
            auto r = client.receive(max_size);
            if (r) return *r;
            return std::unexpected(r.error().to_string());
        }

        void close() override { client.disconnect(); }
        bool is_connected() const override { return client.is_connected(); }
    };

    struct UnixServerAdapter : public protoflow::rpc::transport_interface {
        protoflow::transport::unix::unix_server server;
        explicit UnixServerAdapter(const protoflow::transport::unix::unix_config& cfg)
            : server(cfg) {}

        bool send(std::span<const std::byte>) override { return false; }

        std::expected<std::vector<std::byte>, std::string> receive(size_t) override {
            return std::unexpected(std::string("receive() not supported on server transport"));
        }

        void close() override { server.stop(); }
        bool is_connected() const override { return server.is_listening(); }

        std::unique_ptr<protoflow::rpc::transport_interface> accept() override {
            auto r = server.accept();
            if (!r) return nullptr;
            return std::make_unique<UnixClientAdapter>(std::move(*r));
        }
    };

    if (type == "tcp") {
        if (!port) throw std::invalid_argument("TCP transport requires a port");
        protoflow::transport::tcp::tcp_config cfg{};
        cfg.host = address;
        cfg.port = *port;
        return std::make_unique<TcpServerAdapter>(cfg);
    } else if (type == "unix") {
        protoflow::transport::unix::unix_config cfg{};
        cfg.socket_path = address;
        return std::make_unique<UnixServerAdapter>(cfg);
    } else {
        throw std::invalid_argument("Unsupported transport type: " + type);
    }

}

} // namespace protoflow::mainapp