#include <protoflow/transport/unix.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <poll.h>

#include <protoflow/transport/common.hpp>

namespace protoflow::transport::unix {

// Helper to create unix_error from errno
static unix_error make_error(int err_code, std::string_view context) {
    return unix_error{
        .error_code = err_code,
        .message = std::string(context) + ": " + std::strerror(err_code)
    };
}

static unix_error make_error(std::string_view message) {
    return unix_error{.error_code = -1, .message = std::string(message)};
}

//============================================================================
// unix_client implementation
//============================================================================

unix_client::unix_client(const unix_config& config)
    : config_(config) {}

unix_client::~unix_client() {
    close_socket();
}

unix_client::unix_client(unix_client&& other) noexcept
    : config_(std::move(other.config_))
    , socket_fd_(other.socket_fd_)
    , state_(other.state_) {
    other.socket_fd_ = -1;
    other.state_ = connection_state::disconnected;
}

unix_client& unix_client::operator=(unix_client&& other) noexcept {
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

size_t unix_client::setup_sockaddr(struct sockaddr_un& addr, std::string_view path, bool abstract) {
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    size_t path_len;
    if (abstract) {
        // Abstract namespace (Linux-specific): first byte is null
        addr.sun_path[0] = '\0';
        path_len = std::min(path.size(), sizeof(addr.sun_path) - 1);
        std::memcpy(addr.sun_path + 1, path.data(), path_len);
        path_len += 1; // Include the leading null byte
    } else {
        // Filesystem path
        path_len = std::min(path.size(), sizeof(addr.sun_path) - 1);
        std::memcpy(addr.sun_path, path.data(), path_len);
        addr.sun_path[path_len] = '\0';
    }

    return offsetof(struct sockaddr_un, sun_path) + path_len;
}

std::expected<void, unix_error> unix_client::connect() {
    return connect(config_.socket_path);
}

std::expected<void, unix_error> unix_client::connect(std::string_view socket_path) {
    if (state_ == connection_state::connected) {
        return std::unexpected(make_error("Already connected"));
    }

    close_socket();
    state_ = connection_state::connecting;

    // Create socket
    socket_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
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
    struct sockaddr_un addr{};
    size_t addr_len = setup_sockaddr(addr, socket_path, config_.abstract_namespace);

    // Set non-blocking for timeout support
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

    // Attempt connection
    int result = ::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), 
                          static_cast<socklen_t>(addr_len));
    
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

void unix_client::disconnect() noexcept {
    close_socket();
    state_ = connection_state::disconnected;
}



std::expected<size_t, unix_error> unix_client::send_result(std::span<const std::byte> data) {
    return protoflow::transport::detail::send_result_impl<unix_error>(
        socket_fd_,
        [this]() { this->state_ = connection_state::error; },
        [](int err, std::string_view ctx) { return make_error(err, ctx); },
        [](std::string_view msg) { return make_error(msg); },
        data
    );
}

std::expected<std::vector<std::byte>, unix_error> unix_client::receive_result(size_t max_bytes) {
    std::vector<std::byte> buffer(max_bytes);
    auto result = receive_into(buffer);
    
    if (!result) {
        return std::unexpected(result.error());
    }

    buffer.resize(*result);
    return buffer;
}

bool unix_client::send(std::span<const std::byte> data) {
    auto res = send_result(data);
        return res.has_value() && *res > 0;
}

std::expected<std::vector<std::byte>, std::string> unix_client::receive(size_t max_bytes) {
    auto r = receive_result(max_bytes);
    if (!r) return std::unexpected(r.error().to_string());
    return *r;
}

std::expected<size_t, unix_error> unix_client::receive_into(std::span<std::byte> buffer) {
    return protoflow::transport::detail::receive_into_impl<unix_error>(
        socket_fd_,
        [this]() { this->state_ = connection_state::disconnected; },
        [](int err, std::string_view ctx) { return make_error(err, ctx); },
        [](std::string_view msg) { return make_error(msg); },
        buffer
    );
}

std::expected<void, unix_error> unix_client::send_credentials() {
#ifdef __linux__
    if (state_ != connection_state::connected) {
        return std::unexpected(make_error("Not connected"));
    }

    // Send credentials via ancillary data
    struct ucred cred;
    cred.pid = getpid();
    cred.uid = getuid();
    cred.gid = getgid();

    struct msghdr msg{};
    struct iovec iov{};
    char dummy = '\0';
    
    iov.iov_base = &dummy;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char control[CMSG_SPACE(sizeof(struct ucred))];
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_CREDENTIALS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(struct ucred));
    std::memcpy(CMSG_DATA(cmsg), &cred, sizeof(struct ucred));

    if (sendmsg(socket_fd_, &msg, 0) < 0) {
        return std::unexpected(make_error(errno, "Failed to send credentials"));
    }

    return {};
#else
    return std::unexpected(make_error("Credential passing not supported on this platform"));
#endif
}

std::expected<unix_client::peer_credentials, unix_error> unix_client::receive_credentials() {
#ifdef __linux__
    if (state_ != connection_state::connected) {
        return std::unexpected(make_error("Not connected"));
    }

    struct msghdr msg{};
    struct iovec iov{};
    char dummy;
    
    iov.iov_base = &dummy;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char control[CMSG_SPACE(sizeof(struct ucred))];
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    if (recvmsg(socket_fd_, &msg, 0) < 0) {
        return std::unexpected(make_error(errno, "Failed to receive credentials"));
    }

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_CREDENTIALS) {
        return std::unexpected(make_error("No credentials received"));
    }

    struct ucred cred;
    std::memcpy(&cred, CMSG_DATA(cmsg), sizeof(struct ucred));

    return peer_credentials{
        .pid = cred.pid,
        .uid = cred.uid,
        .gid = cred.gid
    };
#else
    return std::unexpected(make_error("Credential passing not supported on this platform"));
#endif
}

std::string unix_client::peer_path() const {
    if (socket_fd_ < 0) {
        return "";
    }

    struct sockaddr_un addr{};
    socklen_t len = sizeof(addr);
    
    if (getpeername(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        return "";
    }

    if (addr.sun_path[0] == '\0') {
        // Abstract namespace
        return std::string(addr.sun_path + 1, len - offsetof(struct sockaddr_un, sun_path) - 1);
    }

    return addr.sun_path;
}

void unix_client::close_socket() noexcept {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

std::expected<void, unix_error> unix_client::set_socket_options() {
    if (socket_fd_ < 0) {
        return std::unexpected(make_error("Invalid socket"));
    }

#ifdef __linux__
    // Enable credential passing
    int flag = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_PASSCRED, &flag, sizeof(flag)) < 0) {
        return std::unexpected(make_error(errno, "Failed to set SO_PASSCRED"));
    }
#endif

    return {};
}

//============================================================================
// unix_server implementation
//============================================================================

unix_server::unix_server(const unix_config& config)
    : config_(config) {}

unix_server::~unix_server() {
    close_socket();
}

unix_server::unix_server(unix_server&& other) noexcept
    : config_(std::move(other.config_))
    , socket_fd_(other.socket_fd_) {
    other.socket_fd_ = -1;
}

unix_server& unix_server::operator=(unix_server&& other) noexcept {
    if (this != &other) {
        close_socket();
        config_ = std::move(other.config_);
        socket_fd_ = other.socket_fd_;
        other.socket_fd_ = -1;
    }
    return *this;
}

std::expected<void, unix_error> unix_server::listen() {
    return listen(config_.socket_path);
}

std::expected<void, unix_error> unix_server::listen(std::string_view socket_path) {
    if (socket_fd_ >= 0) {
        return std::unexpected(make_error("Already listening"));
    }

    // Remove existing socket file if not using abstract namespace
    if (!config_.abstract_namespace && !socket_path.empty()) {
        ::unlink(std::string(socket_path).c_str());
    }

    // Create socket
    socket_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        return std::unexpected(make_error(errno, "Failed to create socket"));
    }

    // Set socket options
    if (auto result = set_socket_options(); !result) {
        close_socket();
        return result;
    }

    // Bind to address
    struct sockaddr_un addr{};
    size_t addr_len = unix_client::setup_sockaddr(addr, socket_path, config_.abstract_namespace);

    if (::bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), 
               static_cast<socklen_t>(addr_len)) < 0) {
        int err = errno;
        close_socket();
        return std::unexpected(make_error(err, "Bind failed"));
    }

    // Set permissions on filesystem socket
    if (!config_.abstract_namespace && !socket_path.empty()) {
        if (chmod(std::string(socket_path).c_str(), config_.socket_permissions) < 0) {
            int err = errno;
            close_socket();
            return std::unexpected(make_error(err, "Failed to set socket permissions"));
        }
    }

    // Start listening
    if (::listen(socket_fd_, SOMAXCONN) < 0) {
        int err = errno;
        close_socket();
        return std::unexpected(make_error(err, "Listen failed"));
    }

    config_.socket_path = socket_path;
    return {};
}

void unix_server::stop() noexcept {
    close_socket();
}

std::unique_ptr<protoflow::rpc::transport_interface> unix_server::accept() {
    if (socket_fd_ < 0) {
        return nullptr;
    }

    struct sockaddr_un client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = ::accept(socket_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    
    if (client_fd < 0) {
        return nullptr;
    }

    // Create unix_client from accepted socket
    unix_client client;
    client.socket_fd_ = client_fd;
    client.state_ = connection_state::connected;
    client.config_ = config_;

    // Set options on accepted socket
    if (auto result = client.set_socket_options(); !result) {
        ::close(client_fd);
        return nullptr;
    }

    return std::make_unique<unix_client>(std::move(client));
}

std::expected<unix_client, unix_error> unix_server::accept_client() {
    if (socket_fd_ < 0) {
        return std::unexpected(make_error("Not listening"));
    }

    struct sockaddr_un client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = ::accept(socket_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0) {
        return std::unexpected(make_error(errno, "Accept failed"));
    }

    unix_client client;
    client.socket_fd_ = client_fd;
    client.state_ = connection_state::connected;
    client.config_ = config_;

    if (auto result = client.set_socket_options(); !result) {
        ::close(client_fd);
        return std::unexpected(result.error());
    }

    return client;
}

void unix_client::close() {
    disconnect();
}

// The is_connected method is defined inline in the header, so it can be removed.

void unix_server::close_socket() noexcept {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        
        // Remove socket file if not using abstract namespace
        if (!config_.abstract_namespace && !config_.socket_path.empty()) {
            ::unlink(config_.socket_path.c_str());
        }
        
        socket_fd_ = -1;
    }
}

std::expected<void, unix_error> unix_server::set_socket_options() {
    if (socket_fd_ < 0) {
        return std::unexpected(make_error("Invalid socket"));
    }

#ifdef __linux__
    // Enable credential passing
    int flag = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_PASSCRED, &flag, sizeof(flag)) < 0) {
        return std::unexpected(make_error(errno, "Failed to set SO_PASSCRED"));
    }
#endif

    return {};
}

} // namespace protoflow::transport::unix
