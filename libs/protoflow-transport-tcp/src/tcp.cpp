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

namespace protoflow::transport::tcp {

// Helper to create tcp_error from errno
static tcp_error make_error(int err_code, std::string_view context) {
    return tcp_error{
        .error_code = err_code,
        .message = std::string(context) + ": " + std::strerror(err_code)
    };
}

static tcp_error make_error(std::string_view message) {
    return tcp_error{.error_code = -1, .message = std::string(message)};
}

//============================================================================
// tcp_client implementation
//============================================================================

tcp_client::tcp_client(const tcp_config& config)
    : config_(config) {}

tcp_client::~tcp_client() {
    close_socket();
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

std::expected<void, tcp_error> tcp_client::connect() {
    return connect(config_.host, config_.port);
}

std::expected<void, tcp_error> tcp_client::connect(std::string_view host, uint16_t port) {
    if (state_ == connection_state::connected) {
        return std::unexpected(make_error("Already connected"));
    }

    close_socket();
    state_ = connection_state::connecting;

    // Create socket
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        state_ = connection_state::error;
        return std::unexpected(make_error(errno, "Failed to create socket"));
    }

    // Set socket options
    if (auto result = set_socket_options(); !result) {
        close_socket();
        state_ = connection_state::error;
        return result;
    }

    // Setup address
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, std::string(host).c_str(), &addr.sin_addr) <= 0) {
        close_socket();
        state_ = connection_state::error;
        return std::unexpected(make_error("Invalid address"));
    }

    // Set non-blocking for timeout support
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

    // Attempt connection
    int result = ::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    
    if (result < 0 && errno != EINPROGRESS) {
        int err = errno;
        close_socket();
        state_ = connection_state::error;
        return std::unexpected(make_error(err, "Connection failed"));
    }

    // Wait for connection with timeout
    if (result < 0) {
        struct pollfd pfd{};
        pfd.fd = socket_fd_;
        pfd.events = POLLOUT;

        int poll_result = poll(&pfd, 1, static_cast<int>(config_.connect_timeout_ms));
        
        if (poll_result == 0) {
            close_socket();
            state_ = connection_state::error;
            return std::unexpected(make_error("Connection timeout"));
        }
        
        if (poll_result < 0) {
            int err = errno;
            close_socket();
            state_ = connection_state::error;
            return std::unexpected(make_error(err, "Poll failed"));
        }

        // Check for errors
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            int err = errno;
            close_socket();
            state_ = connection_state::error;
            return std::unexpected(make_error(err, "getsockopt failed"));
        }

        if (error != 0) {
            close_socket();
            state_ = connection_state::error;
            return std::unexpected(make_error(error, "Connection failed"));
        }
    }

    // Restore blocking mode
    fcntl(socket_fd_, F_SETFL, flags);

    state_ = connection_state::connected;
    return {};
}

void tcp_client::disconnect() noexcept {
    close_socket();
    state_ = connection_state::disconnected;
}

std::expected<size_t, tcp_error> tcp_client::send(std::span<const std::byte> data) {
    if (state_ != connection_state::connected) {
        return std::unexpected(make_error("Not connected"));
    }

    if (data.empty()) {
        return 0;
    }

    ssize_t sent = ::send(socket_fd_, data.data(), data.size(), MSG_NOSIGNAL);
    
    if (sent < 0) {
        int err = errno;
        if (err == EAGAIN) {
            return std::unexpected(make_error(err, "Send would block"));
        }
        state_ = connection_state::error;
        return std::unexpected(make_error(err, "Send failed"));
    }

    return static_cast<size_t>(sent);
}

std::expected<std::vector<std::byte>, tcp_error> tcp_client::receive(size_t max_bytes) {
    std::vector<std::byte> buffer(max_bytes);
    auto result = receive_into(buffer);
    
    if (!result) {
        return std::unexpected(result.error());
    }

    buffer.resize(*result);
    return buffer;
}

std::expected<size_t, tcp_error> tcp_client::receive_into(std::span<std::byte> buffer) {
    if (state_ != connection_state::connected) {
        return std::unexpected(make_error("Not connected"));
    }

    if (buffer.empty()) {
        return 0;
    }

    ssize_t received = ::recv(socket_fd_, buffer.data(), buffer.size(), 0);
    
    if (received < 0) {
        int err = errno;
        if (err == EAGAIN) {
            return std::unexpected(make_error(err, "Receive would block"));
        }
        state_ = connection_state::error;
        return std::unexpected(make_error(err, "Receive failed"));
    }

    if (received == 0) {
        // Connection closed by peer
        state_ = connection_state::disconnected;
        return std::unexpected(make_error("Connection closed by peer"));
    }

    return static_cast<size_t>(received);
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
    return listen(config_.port);
}

std::expected<void, tcp_error> tcp_server::listen(uint16_t port) {
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
    addr.sin_port = htons(port);

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

    config_.port = port;
    return {};
}

void tcp_server::stop() noexcept {
    close_socket();
}

std::expected<tcp_client, tcp_error> tcp_server::accept() {
    if (socket_fd_ < 0) {
        return std::unexpected(make_error("Not listening"));
    }

    struct sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = ::accept(socket_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    
    if (client_fd < 0) {
        return std::unexpected(make_error(errno, "Accept failed"));
    }

    // Create tcp_client from accepted socket
    tcp_client client;
    client.socket_fd_ = client_fd;
    client.state_ = connection_state::connected;
    client.config_ = config_;

    // Set options on accepted socket
    if (auto result = client.set_socket_options(); !result) {
        ::close(client_fd);
        return std::unexpected(result.error());
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
