#pragma once

#include <string>
#include <protoflow/rpc/protocol.hpp>
#include <string_view>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>
#include <cstddef>
#include <unistd.h>

namespace protoflow::transport::tcp {

// TCP connection state
enum class connection_state : uint8_t {
    disconnected,
    connecting,
    connected,
    error
};

// TCP socket configuration
struct tcp_config {
    std::string host{"localhost"};
    uint16_t port{8080};
    uint32_t connect_timeout_ms{5000};
    uint32_t read_timeout_ms{1000};
    uint32_t write_timeout_ms{1000};
    size_t buffer_size{8192};
    bool reuse_addr{true};
    bool nodelay{true};  // Disable Nagle's algorithm
};

// Error types
struct tcp_error {
    int error_code{0};
    std::string message;
    
    [[nodiscard]] std::string to_string() const {
        return "TCP Error " + std::to_string(error_code) + ": " + message;
    }
};

// TCP client for outbound connections
class tcp_client : public protoflow::rpc::transport_interface {
public:
    tcp_client();
    explicit tcp_client(const tcp_config& config);
    tcp_client(const std::string& host, uint16_t port):
        config_{.host = host, .port = port} {}
    ~tcp_client();

    // Non-copyable, movable
    tcp_client(const tcp_client&) = delete;
    tcp_client& operator=(const tcp_client&) = delete;
    tcp_client(tcp_client&& other) noexcept;
    tcp_client& operator=(tcp_client&& other) noexcept;

    // Connection management
    [[nodiscard]] std::expected<void, tcp_error> connect();
    [[nodiscard]] std::expected<void, tcp_error> connect(std::string_view host, uint16_t port);
    void disconnect() noexcept;
    
    [[nodiscard]] bool is_connected() const noexcept { return state_ == connection_state::connected; }
    [[nodiscard]] connection_state state() const noexcept { return state_; }

    // I/O operations
    // transport_interface-compatible methods
    bool send(std::span<const std::byte> data) override;
    std::expected<std::vector<std::byte>, std::string> receive(size_t max_bytes) override;
    // Close transport (transport_interface)
    void close() override;

    // Backwards-compatible helpers that expose richer error info
    [[nodiscard]] std::expected<size_t, tcp_error> send_result(std::span<const std::byte> data);
    [[nodiscard]] std::expected<std::vector<std::byte>, tcp_error> receive_result(size_t max_bytes);
    [[nodiscard]] std::expected<size_t, tcp_error> receive_into(std::span<std::byte> buffer);

    // Configuration
    [[nodiscard]] const tcp_config& config() const noexcept { return config_; }
    void set_config(const tcp_config& config) noexcept { config_ = config; }

    // Socket information
    [[nodiscard]] int socket_fd() const noexcept { return socket_fd_; }
    [[nodiscard]] std::string peer_address() const;
    [[nodiscard]] uint16_t peer_port() const;
    // Adopt an existing connected socket (used by servers accepting clients)
    void adopt_socket(int fd) noexcept;

private:
    void close_socket() noexcept;
    [[nodiscard]] std::expected<void, tcp_error> set_socket_options();

    tcp_config config_;
    int socket_fd_{-1};
    connection_state state_{connection_state::disconnected};
};

// TCP server for inbound connections
class tcp_server : public protoflow::rpc::transport_interface {
public:
    tcp_server();
    explicit tcp_server(const tcp_config& config);
    ~tcp_server();

    // Non-copyable, movable
    tcp_server(const tcp_server&) = delete;
    tcp_server& operator=(const tcp_server&) = delete;
    tcp_server(tcp_server&& other) noexcept;
    tcp_server& operator=(tcp_server&& other) noexcept;

    // Server lifecycle
    [[nodiscard]] std::expected<void, tcp_error> listen();
    [[nodiscard]] std::expected<void, tcp_error> listen(uint16_t port);
    void stop() noexcept;
    
    [[nodiscard]] bool is_listening() const noexcept { return socket_fd_ >= 0; }

    // Accessors for tests and external use
    [[nodiscard]] int socket_fd() const noexcept { return socket_fd_; }
    [[nodiscard]] const tcp_config& config() const noexcept { return config_; }
    void set_config(const tcp_config& config) noexcept { config_ = config; }
    [[nodiscard]] uint16_t listening_port() const noexcept { return config_.port; }

    // transport_interface methods (server)
    // Server does not support direct send/receive; provide no-op implementations
    bool send(std::span<const std::byte> data) override { return ::write(socket_fd_, data.data(), data.size()); }
    std::expected<std::vector<std::byte>, std::string> receive(size_t max_bytes) override {
        auto buffer = std::vector<std::byte>(max_bytes);
        auto data = ::read(socket_fd_, buffer.data(), max_bytes);
        if (data < 0) {
            return std::unexpected(std::string("receive() failed"));
        }
        buffer.resize(static_cast<size_t>(data));
        return buffer;
    }
    [[nodiscard]] std::unique_ptr<protoflow::rpc::transport_interface> accept() override;
    void close() override { stop(); }
    bool is_connected() const override { return is_listening(); }

    // Backwards-compatible accept returning expected client
    [[nodiscard]] std::expected<tcp_client, tcp_error> accept_client();

private:
    void close_socket() noexcept;
    [[nodiscard]] std::expected<void, tcp_error> set_socket_options();

    tcp_config config_;
    int socket_fd_{-1};
    connection_state state_{connection_state::disconnected};
};

} // namespace protoflow::transport::tcp
