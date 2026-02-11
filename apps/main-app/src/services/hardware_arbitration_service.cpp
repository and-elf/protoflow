#include "services/hardware_arbitration_service.hpp"
#include <protoflow/logging/macros.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace protoflow::mainapp {

HardwareArbitrationService::HardwareArbitrationService(config::HardwareConfig resources)
    : resources_(std::move(resources))
    , last_timeout_check_(std::chrono::steady_clock::now())
{
}

HardwareArbitrationService::~HardwareArbitrationService() {
    // Close all open hardware sessions
    for (auto& [handle, session] : sessions_) {
        if (session.fd >= 0) {
            close_device(session.fd);
        }
    }
}

void HardwareArbitrationService::start() {
    PROTOFLOW_LOG_INFO(*this, "Started");
    PROTOFLOW_LOG_INFO(*this, "Managing " << resources_.size() << " hardware resources");
    
    for (const auto& [name, res] : resources_) {
        PROTOFLOW_LOG_INFO(*this, "  - " << name << ": " << res.device 
                          << " (mode: " << res.mode << ")");
    }
}

void HardwareArbitrationService::stop() {
    PROTOFLOW_LOG_INFO(*this, "Stopped");
    
    // Close all sessions
    for (auto& [handle, session] : sessions_) {
        if (session.fd >= 0) {
            close_device(session.fd);
        }
    }
    sessions_.clear();
}

void HardwareArbitrationService::poll() {
    // Check for expired sessions periodically
    auto now = std::chrono::steady_clock::now();
    if (now - last_timeout_check_ > std::chrono::seconds(5)) {
        check_timeouts();
        last_timeout_check_ = now;
    }

    // Call base class poll for message handling
    service::Service::poll();
}

std::expected<hw_protocol::hw_handle, hw_protocol::error_code> HardwareArbitrationService::request_access_internal(
    const std::string& app_name,
    const std::string& resource,
    hw_protocol::access_mode mode,
    std::chrono::seconds timeout
) {
    // Check if resource exists
    auto res_it = resources_.find(resource);
    if (res_it == resources_.end()) {
        PROTOFLOW_LOG_WARN(*this, "Access denied for " << app_name 
                           << ": resource '" << resource << "' not found");
        return std::unexpected(hw_protocol::error_code::resource_not_found);
    }

    // Check if resource can be acquired
    if (!can_acquire(resource, mode)) {
        PROTOFLOW_LOG_WARN(*this, "Access denied for " << app_name 
                           << ": resource '" << resource << "' already in use");
        return std::unexpected(hw_protocol::error_code::resource_busy);
    }

    // Open hardware device
    auto fd_result = open_device(res_it->second.device, mode);
    if (!fd_result) {
        return std::unexpected(fd_result.error());
    }

    // Create session
    HardwareSession session;
    session.handle = next_handle_++;
    session.app_name = app_name;
    session.resource = resource;
    session.mode = mode;
    session.acquired_at = std::chrono::steady_clock::now();
    session.timeout = timeout;
    session.fd = *fd_result;

    sessions_[session.handle] = std::move(session);

    PROTOFLOW_LOG_INFO(*this, "Access granted to " << app_name 
                       << ": resource '" << resource << "' (handle " << session.handle << ")");

    return session.handle;
}

hw_protocol::error_code HardwareArbitrationService::release_access_internal(hw_protocol::hw_handle handle) {
    auto* session = get_session(handle);
    if (!session) {
        return hw_protocol::error_code::invalid_handle;
    }

    PROTOFLOW_LOG_INFO(*this, "Releasing handle " << handle 
                       << " for " << session->app_name);

    // Close device
    if (session->fd >= 0) {
        close_device(session->fd);
    }

    // Remove session
    sessions_.erase(handle);

    return hw_protocol::error_code::success;
}

std::expected<std::vector<std::byte>, hw_protocol::error_code> HardwareArbitrationService::read_internal(
    hw_protocol::hw_handle handle,
    size_t max_length
) {
    auto* session = get_session(handle);
    if (!session) {
        return std::unexpected(hw_protocol::error_code::invalid_handle);
    }

    // Check capability
    auto res_it = resources_.find(session->resource);
    if (res_it == resources_.end() || 
        !(res_it->second.capabilities & hw_protocol::capability_flags::can_read)) {
        return std::unexpected(hw_protocol::error_code::invalid_operation);
    }

    // Enforce buffer limit
    if (max_length > hw_protocol::max_buffer_size) {
        max_length = hw_protocol::max_buffer_size;
    }

    // Perform read
    std::vector<std::byte> buffer(max_length);
    ssize_t bytes_read = ::read(session->fd, buffer.data(), max_length);
    
    if (bytes_read < 0) {
        return std::unexpected(hw_protocol::error_code::io_error);
    }

    buffer.resize(static_cast<size_t>(bytes_read));
    return buffer;
}

std::expected<size_t, hw_protocol::error_code> HardwareArbitrationService::write_internal(
    hw_protocol::hw_handle handle,
    std::span<const std::byte> data
) {
    auto* session = get_session(handle);
    if (!session) {
        return std::unexpected(hw_protocol::error_code::invalid_handle);
    }

    // Check capability
    auto res_it = resources_.find(session->resource);
    if (res_it == resources_.end() || 
        !(res_it->second.capabilities & hw_protocol::capability_flags::can_write)) {
        return std::unexpected(hw_protocol::error_code::invalid_operation);
    }

    // Enforce buffer limit
    size_t write_size = std::min(data.size(), hw_protocol::max_buffer_size);

    // Perform write
    ssize_t bytes_written = ::write(session->fd, data.data(), write_size);
    
    if (bytes_written < 0) {
        return std::unexpected(hw_protocol::error_code::io_error);
    }

    return static_cast<size_t>(bytes_written);
}

std::expected<hw_protocol::ioctl_result, hw_protocol::error_code> HardwareArbitrationService::ioctl_internal(
    hw_protocol::hw_handle handle,
    uint32_t request,
    std::span<const std::byte> args
) {
    auto* session = get_session(handle);
    if (!session) {
        return std::unexpected(hw_protocol::error_code::invalid_handle);
    }

    // Check capability
    auto res_it = resources_.find(session->resource);
    if (res_it == resources_.end() || 
        !(res_it->second.capabilities & hw_protocol::capability_flags::can_ioctl)) {
        return std::unexpected(hw_protocol::error_code::invalid_operation);
    }

    // Perform ioctl (simplified - actual implementation would need proper arg handling)
    int result = ::ioctl(session->fd, request, args.data());
    
    if (result < 0) {
        return std::unexpected(hw_protocol::error_code::io_error);
    }

    hw_protocol::ioctl_result ioctl_result;
    ioctl_result.result = static_cast<uint32_t>(result);
    // In a full implementation, we would copy output data here
    
    return ioctl_result;
}

void HardwareArbitrationService::handle(messaging::Message&& msg) {
    // Handle incoming hardware access messages from RPC clients
    // This would parse hardware access requests and route to appropriate methods
    (void)msg; // TODO: Implement message handling
}

std::vector<messaging::Message> HardwareArbitrationService::generate_outbound() {
    auto messages = std::move(outbound_);
    outbound_.clear();
    return messages;
}

void HardwareArbitrationService::check_timeouts() {
    auto now = std::chrono::steady_clock::now();
    
    std::vector<hw_protocol::hw_handle> expired;
    
    for (const auto& [handle, session] : sessions_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - session.acquired_at
        );
        
        if (elapsed > session.timeout) {
            PROTOFLOW_LOG_WARN(*this, "Session timeout for handle " 
                               << handle << " (" << session.app_name << ")");
            expired.push_back(handle);
        }
    }
    
    for (auto handle : expired) {
        (void)release_access_internal(handle);
    }
}

HardwareSession* HardwareArbitrationService::get_session(hw_protocol::hw_handle handle) {
    auto it = sessions_.find(handle);
    if (it != sessions_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool HardwareArbitrationService::can_acquire(
    const std::string& resource,
    hw_protocol::access_mode mode
) const {
    auto res_it = resources_.find(resource);
    if (res_it == resources_.end()) {
        return false;
    }

    const auto& resource_config = res_it->second;

    // Count current sessions for this resource
    int active_sessions = 0;
    bool has_exclusive = false;

    for (const auto& [handle, session] : sessions_) {
        if (session.resource == resource) {
            active_sessions++;
            if (session.mode == hw_protocol::access_mode::exclusive) {
                has_exclusive = true;
            }
        }
    }

    // Exclusive mode: no other sessions allowed
    if (mode == hw_protocol::access_mode::exclusive) {
        return active_sessions == 0;
    }

    // Shared mode: check limits and no exclusive session
    if (resource_config.mode == "shared") {
        return !has_exclusive && active_sessions < resource_config.max_clients;
    }

    // Resource configured as exclusive
    return active_sessions == 0;
}

std::expected<int, hw_protocol::error_code> HardwareArbitrationService::open_device(
    const std::string& device,
    hw_protocol::access_mode mode
) {
    int flags = O_RDWR; // Most hardware devices need read/write
    if (mode == hw_protocol::access_mode::exclusive) {
        flags |= O_EXCL;
    }

    int fd = ::open(device.c_str(), flags);
    if (fd < 0) {
        PROTOFLOW_LOG_ERROR(*this, "Failed to open device " << device 
                            << ": " << strerror(errno));
        
        if (errno == EBUSY) {
            return std::unexpected(hw_protocol::error_code::resource_busy);
        } else if (errno == ENOENT) {
            return std::unexpected(hw_protocol::error_code::resource_not_found);
        } else if (errno == EACCES) {
            return std::unexpected(hw_protocol::error_code::permission_denied);
        }
        
        return std::unexpected(hw_protocol::error_code::io_error);
    }

    return fd;
}

void HardwareArbitrationService::close_device(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

} // namespace protoflow::mainapp
