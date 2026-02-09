#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>
#include <cstddef>
#include <sys/un.h>

namespace protoflow::transport::unix {

// Unix socket connection state
enum class connection_state : uint8_t {
    disconnected,
    connecting,
    connected,
    error
};

// Unix socket configuration
struct unix_config {
    std::string socket_path{"/tmp/protoflow.sock"};
    uint32_t connect_timeout_ms{5000};
    uint32_t read_timeout_ms{1000};
    uint32_t write_timeout_ms{1000};
    size_t buffer_size{8192};
    mode_t socket_permissions{0600};  // Owner read/write only
    bool abstract_namespace{false};   // Use abstract namespace (Linux)
};

// Error types
struct unix_error {
    int error_code{0};
    std::string message;
    
    [[nodiscard]] std::string to_string() const {
        return "Unix Socket Error " + std::to_string(error_code) + ": " + message;
    }
};

// Unix socket client for outbound connections
class unix_client {
public:
    // Friend declaration for unix_server
    friend class unix_server;

    unix_client() = default;
    explicit unix_client(const unix_config& config);
    ~unix_client();

    // Non-copyable, movable
    unix_client(const unix_client&) = delete;
    unix_client& operator=(const unix_client&) = delete;
    unix_client(unix_client&& other) noexcept;
    unix_client& operator=(unix_client&& other) noexcept;

    // Connection management
    [[nodiscard]] std::expected<void, unix_error> connect();
    [[nodiscard]] std::expected<void, unix_error> connect(std::string_view socket_path);
    void disconnect() noexcept;
    
    [[nodiscard]] bool is_connected() const noexcept { return state_ == connection_state::connected; }
    [[nodiscard]] connection_state state() const noexcept { return state_; }

    // I/O operations
    [[nodiscard]] std::expected<size_t, unix_error> send(std::span<const std::byte> data);
    [[nodiscard]] std::expected<std::vector<std::byte>, unix_error> receive(size_t max_bytes);
    [[nodiscard]] std::expected<size_t, unix_error> receive_into(std::span<std::byte> buffer);

    // Credential passing (Linux-specific)
    [[nodiscard]] std::expected<void, unix_error> send_credentials();
    
    struct peer_credentials {
        pid_t pid;
        uid_t uid;
        gid_t gid;
    };
    [[nodiscard]] std::expected<peer_credentials, unix_error> receive_credentials();

    // Configuration
    [[nodiscard]] const unix_config& config() const noexcept { return config_; }
    void set_config(const unix_config& config) noexcept { config_ = config; }

    // Socket information
    [[nodiscard]] int socket_fd() const noexcept { return socket_fd_; }
    [[nodiscard]] std::string peer_path() const;

private:
    void close_socket() noexcept;
    [[nodiscard]] std::expected<void, unix_error> set_socket_options();
    [[nodiscard]] static size_t setup_sockaddr(struct sockaddr_un& addr, std::string_view path, bool abstract);

    unix_config config_;
    int socket_fd_{-1};
    connection_state state_{connection_state::disconnected};
};

// Unix socket server for inbound connections
class unix_server {
public:
    unix_server() = default;
    explicit unix_server(const unix_config& config);
    ~unix_server();

    // Non-copyable, movable
    unix_server(const unix_server&) = delete;
    unix_server& operator=(const unix_server&) = delete;
    unix_server(unix_server&& other) noexcept;
    unix_server& operator=(unix_server&& other) noexcept;

    // Server lifecycle
    [[nodiscard]] std::expected<void, unix_error> listen();
    [[nodiscard]] std::expected<void, unix_error> listen(std::string_view socket_path);
    void stop() noexcept;
    
    [[nodiscard]] bool is_listening() const noexcept { return socket_fd_ >= 0; }

    // Accept connections
    [[nodiscard]] std::expected<unix_client, unix_error> accept();

    // Configuration
    [[nodiscard]] const unix_config& config() const noexcept { return config_; }
    void set_config(const unix_config& config) noexcept { config_ = config; }

    // Socket information
    [[nodiscard]] int socket_fd() const noexcept { return socket_fd_; }
    [[nodiscard]] std::string socket_path() const noexcept { return config_.socket_path; }

private:
    void close_socket() noexcept;
    [[nodiscard]] std::expected<void, unix_error> set_socket_options();

    unix_config config_;
    int socket_fd_{-1};
};

} // namespace protoflow::transport::unix
