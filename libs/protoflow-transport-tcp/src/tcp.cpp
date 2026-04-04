#include <protoflow/transport/tcp.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <poll.h>

#include <protoflow/transport/common.hpp>

namespace protoflow::transport::tcp {

// Helper to create tcp_error from errno
tcp_error make_error(int err_code, std::string_view context) {
    return tcp_error{
        .error_code = err_code,
        .message = std::string(context) + ": " + std::strerror(err_code)
    };
}

tcp_error make_error(std::string_view message) {
    return tcp_error{.error_code = -1, .message = std::string(message)};
}

//============================================================================
// tcp_client implementation
//============================================================================

tcp_client::tcp_client(const tcp_config& config)
    : config_(config) {}

tcp_client::tcp_client()
    : config_() {}

std::expected<void, tcp_error> tcp_client::connect() {
    return connect(config_.host, config_.port);
}

std::expected<void, tcp_error> tcp_client::connect(std::string_view host, uint16_t port) {
    // Close existing socket if any
    close_socket();

    // Create socket
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return std::unexpected(make_error(errno, "Failed to create socket"));
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, std::string(host).c_str(), &addr.sin_addr) <= 0) {
        ::close(fd);
        return std::unexpected(make_error("Invalid address"));
    }

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        int err = errno;
        ::close(fd);
        return std::unexpected(make_error(err, "Connect failed"));
    }

    socket_fd_ = fd;
    state_ = connection_state::connected;

    if (auto res = set_socket_options(); !res) {
        close_socket();
        return res;
    }

    return {};
}

tcp_client::~tcp_client() {
    close_socket();
}

void tcp_client::adopt_socket(int fd) noexcept {
    socket_fd_ = fd;
    state_ = connection_state::connected;
}

tcp_client::tcp_client(tcp_client&& other) noexcept
    : config_(std::move(other.config_))
    , socket_fd_(other.socket_fd_)
    , state_(other.state_) {
    other.socket_fd_ = -1;
    other.state_ = connection_state::disconnected;
}

tcp_client& tcp_client::operator=(tcp_client&& other) noexcept {
    if (this != &other) {
        close_socket();
        config_ = std::move(other.config_);
        socket_fd_ = other.socket_fd_;
        state_ = other.state_;
        other.socket_fd_ = -1;
        other.state_ = connection_state::disconnected;
    }
    return *this;
}

std::expected<void, tcp_error> tcp_server::listen(uint16_t port) {
    config_.port = port;
    return listen();
}

void tcp_server::stop() noexcept {
    close_socket();
}
// Backwards-compatible detailed send returning bytes or tcp_error
std::expected<size_t, tcp_error> tcp_client::send_result(std::span<const std::byte> data) {
    return protoflow::transport::detail::send_result_impl<tcp_error>(
        socket_fd_,
        [this]() { this->state_ = connection_state::error; },
        [](int err, std::string_view ctx) { return make_error(err, ctx); },
        [](std::string_view msg) { return make_error(msg); },
        data
    );
}

// transport_interface-compatible send: return true on success
bool tcp_client::send(std::span<const std::byte> data) {
    auto res = send_result(data);
    return res.has_value() && *res > 0;
}

// Backwards-compatible detailed receive returning tcp_error
std::expected<std::vector<std::byte>, tcp_error> tcp_client::receive_result(size_t max_bytes) {
    std::vector<std::byte> buffer(max_bytes);
    auto result = receive_into(buffer);

    if (!result) {
        return std::unexpected(result.error());
    }

    buffer.resize(*result);
    return buffer;
}

// transport_interface-compatible receive: convert tcp_error -> string
std::expected<std::vector<std::byte>, std::string> tcp_client::receive(size_t max_bytes) {
    auto r = receive_result(max_bytes);
    if (!r) return std::unexpected(r.error().to_string());
    return *r;
}

void tcp_client::close() {
    disconnect();
}

void tcp_client::disconnect() noexcept {
    close_socket();
    state_ = connection_state::disconnected;
}

std::expected<size_t, tcp_error> tcp_client::receive_into(std::span<std::byte> buffer) {
    return protoflow::transport::detail::receive_into_impl<tcp_error>(
        socket_fd_,
        [this]() { this->state_ = connection_state::disconnected; },
        [](int err, std::string_view ctx) { return make_error(err, ctx); },
        [](std::string_view msg) { return make_error(msg); },
        buffer
    );
}

std::string tcp_client::peer_address() const {
    if (socket_fd_ < 0) {
        return "";
    }

    struct sockaddr_in addr{};
    socklen_t len = sizeof(addr);

    if (getpeername(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        return "";
    }

    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    return buf;
}

uint16_t tcp_client::peer_port() const {
    if (socket_fd_ < 0) {
        return 0;
    }

    struct sockaddr_in addr{};
    socklen_t len = sizeof(addr);

    if (getpeername(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        return 0;
    }

    return ntohs(addr.sin_port);
}

void tcp_client::close_socket() noexcept {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

std::expected<void, tcp_error> tcp_client::set_socket_options() {
    if (socket_fd_ < 0) {
        return std::unexpected(make_error("Invalid socket"));
    }

    // Set TCP_NODELAY if requested
    if (config_.nodelay) {
        int flag = 1;
        if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            return std::unexpected(make_error(errno, "Failed to set TCP_NODELAY"));
        }
    }

    // Set SO_REUSEADDR if requested
    if (config_.reuse_addr) {
        int flag = 1;
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
            return std::unexpected(make_error(errno, "Failed to set SO_REUSEADDR"));
        }
    }

    return {};
}

//============================================================================
// tcp_server implementation
//============================================================================

tcp_server::tcp_server()
    : config_() {}

tcp_server::tcp_server(const tcp_config& config)
    : config_(config) {}

tcp_server::~tcp_server() {
    close_socket();
}

tcp_server::tcp_server(tcp_server&& other) noexcept
    : config_(std::move(other.config_))
    , socket_fd_(other.socket_fd_) {
    other.socket_fd_ = -1;
}

tcp_server& tcp_server::operator=(tcp_server&& other) noexcept {
    if (this != &other) {
        close_socket();
        config_ = std::move(other.config_);
        socket_fd_ = other.socket_fd_;
        other.socket_fd_ = -1;
    }
    return *this;
}

std::expected<void, tcp_error> tcp_server::listen() {
    if (socket_fd_ >= 0) {
        return std::unexpected(make_error("Already listening"));
    }

    // Create socket
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        return std::unexpected(make_error(errno, "Failed to create socket"));
    }

    // Set socket options
    if (auto result = set_socket_options(); !result) {
        close_socket();
        return result;
    }

    // Bind to address
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config_.port);

    if (::bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        int err = errno;
        close_socket();
        return std::unexpected(make_error(err, "Bind failed"));
    }

    // Start listening
    if (::listen(socket_fd_, SOMAXCONN) < 0) {
        int err = errno;
        close_socket();
        return std::unexpected(make_error(err, "Listen failed"));
    }

    // Set non-blocking so accept() returns immediately when no pending connections
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);
    }

    return {};
}

std::unique_ptr<protoflow::rpc::transport_interface> tcp_server::accept() {
    // Non-blocking accept for event loop use: poll with 0ms timeout
    if (socket_fd_ < 0) return nullptr;

    struct pollfd pfd{};
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;

    int poll_result = ::poll(&pfd, 1, 0);  // non-blocking check
    if (poll_result <= 0) return nullptr;

    struct sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = ::accept(socket_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0) return nullptr;

    tcp_config client_config = config_;
    auto client = std::make_unique<tcp_client>(client_config);
    client->adopt_socket(client_fd);
    return client;
}

std::expected<tcp_client, tcp_error> tcp_server::accept_client() {
    if (socket_fd_ < 0) {
        return std::unexpected(make_error("Not listening"));
    }

    // Use poll() to wait for a pending connection with timeout.
    // This handles both blocking and non-blocking sockets correctly.
    struct pollfd pfd{};
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;

    int poll_result = ::poll(&pfd, 1, static_cast<int>(config_.connect_timeout_ms));
    if (poll_result < 0) {
        return std::unexpected(make_error(errno, "Poll failed"));
    }
    if (poll_result == 0) {
        return std::unexpected(make_error("No pending connections"));
    }

    struct sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = ::accept(socket_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0) {
        // EAGAIN/EWOULDBLOCK means no pending connections (non-blocking socket)
        if (errno == EAGAIN) {
            return std::unexpected(make_error(errno, "No pending connections"));
        }
        return std::unexpected(make_error(errno, "Accept failed"));
    }

    tcp_config client_config = config_;
    tcp_client client(client_config);
    client.set_config(client_config);
    client.adopt_socket(client_fd);

    // Set options on accepted socket (mirror tcp_client::set_socket_options)
    client.adopt_socket(client_fd);

    if (client.socket_fd() < 0) {
        ::close(client_fd);
        return std::unexpected(make_error("Invalid socket"));
    }

    if (client.config().nodelay) {
        int flag = 1;
        if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            ::close(client_fd);
            return std::unexpected(make_error(errno, "Failed to set TCP_NODELAY"));
        }
    }

    if (client.config().reuse_addr) {
        int flag = 1;
        if (setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
            ::close(client_fd);
            return std::unexpected(make_error(errno, "Failed to set SO_REUSEADDR"));
        }
    }

    return client;
}

void tcp_server::close_socket() noexcept {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

std::expected<void, tcp_error> tcp_server::set_socket_options() {
    if (socket_fd_ < 0) {
        return std::unexpected(make_error("Invalid socket"));
    }

    // Set SO_REUSEADDR
    if (config_.reuse_addr) {
        int flag = 1;
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
            return std::unexpected(make_error(errno, "Failed to set SO_REUSEADDR"));
        }
    }

    return {};
}

} // namespace protoflow::transport::tcp
