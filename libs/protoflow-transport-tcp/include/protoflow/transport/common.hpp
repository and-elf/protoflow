#pragma once

#include <expected>
#include <span>
#include <vector>
#include <string>
#include <system_error>
#include <functional>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>

namespace protoflow::transport::detail {

// Helper to implement send_result for both TCP and Unix transports.
template<typename ErrorT>
inline std::expected<size_t, ErrorT> send_result_impl(
    int socket_fd,
    const std::function<void()>& set_error_state,
    const std::function<ErrorT(int, std::string_view)>& make_errno_error,
    const std::function<ErrorT(std::string_view)>& make_str_error,
    std::span<const std::byte> data)
{
    if (socket_fd < 0) {
        return std::unexpected(make_str_error("Not connected"));
    }

    if (data.empty()) {
        return static_cast<size_t>(0);
    }

    ssize_t sent = ::send(socket_fd, data.data(), data.size(), MSG_NOSIGNAL);
    if (sent < 0) {
        int err = errno;
        if (err == EAGAIN) {
            return std::unexpected(make_errno_error(err, "Send would block"));
        }
        set_error_state();
        return std::unexpected(make_errno_error(err, "Send failed"));
    }

    return static_cast<size_t>(sent);
}

// Helper to implement receive_into for both TCP and Unix transports.
template<typename ErrorT>
inline std::expected<size_t, ErrorT> receive_into_impl(
    int socket_fd,
    const std::function<void()>& set_error_state,
    const std::function<ErrorT(int, std::string_view)>& make_errno_error,
    const std::function<ErrorT(std::string_view)>& make_str_error,
    std::span<std::byte> buffer)
{
    if (socket_fd < 0) {
        return std::unexpected(make_str_error("Not connected"));
    }

    if (buffer.empty()) {
        return static_cast<size_t>(0);
    }

    ssize_t received = ::recv(socket_fd, buffer.data(), buffer.size(), 0);
    if (received < 0) {
        int err = errno;
        if (err == EAGAIN) {
            return std::unexpected(make_errno_error(err, "Receive would block"));
        }
        set_error_state();
        return std::unexpected(make_errno_error(err, "Receive failed"));
    }

    if (received == 0) {
        // Connection closed by peer
        set_error_state();
        return std::unexpected(make_str_error("Connection closed by peer"));
    }

    return static_cast<size_t>(received);
}

} // namespace protoflow::transport::detail
