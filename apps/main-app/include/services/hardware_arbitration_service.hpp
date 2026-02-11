#pragma once

#include <protoflow/service.hpp>
#include <protoflow/fsm.hpp>
#include <protoflow/hw_protocol/protocol.hpp>
#include <protoflow/config/hardware_config.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <optional>
#include <memory>

namespace protoflow::mainapp {

/// Active hardware access session
struct HardwareSession {
    hw_protocol::hw_handle handle;
    std::string app_name;
    std::string resource;
    hw_protocol::access_mode mode;
    std::chrono::steady_clock::time_point acquired_at;
    std::chrono::seconds timeout;
    
    // Platform-specific file descriptor or handle
    int fd = -1;
};

/// Service managing hardware resource arbitration
/// Proxies all hardware I/O for registered apps
class HardwareArbitrationService : public service::Service {
public:
    explicit HardwareArbitrationService(config::HardwareConfig resources);
    ~HardwareArbitrationService() override;

    void start() override;
    void stop() override;
    void poll() override;

protected:
    void handle(messaging::Message&& msg) override;
    std::vector<messaging::Message> generate_outbound() override;

private:
    /// Check for expired sessions
    void check_timeouts();

    /// Internal: Request hardware access
    [[nodiscard]] std::expected<hw_protocol::hw_handle, hw_protocol::error_code> request_access_internal(
        const std::string& app_name,
        const std::string& resource,
        hw_protocol::access_mode mode,
        std::chrono::seconds timeout
    );

    /// Internal: Release hardware access
    [[nodiscard]] hw_protocol::error_code release_access_internal(hw_protocol::hw_handle handle);

    /// Internal: Read from hardware
    [[nodiscard]] std::expected<std::vector<std::byte>, hw_protocol::error_code> read_internal(
        hw_protocol::hw_handle handle,
        size_t max_length
    );

    /// Internal: Write to hardware
    [[nodiscard]] std::expected<size_t, hw_protocol::error_code> write_internal(
        hw_protocol::hw_handle handle,
        std::span<const std::byte> data
    );

    /// Internal: Perform IOCTL operation
    [[nodiscard]] std::expected<hw_protocol::ioctl_result, hw_protocol::error_code> ioctl_internal(
        hw_protocol::hw_handle handle,
        uint32_t request,
        std::span<const std::byte> args
    );

    /// Validate handle
    [[nodiscard]] HardwareSession* get_session(hw_protocol::hw_handle handle);

    /// Check if resource can be acquired
    [[nodiscard]] bool can_acquire(
        const std::string& resource,
        hw_protocol::access_mode mode
    ) const;

    /// Platform-specific: open hardware device
    [[nodiscard]] std::expected<int, hw_protocol::error_code> open_device(
        const std::string& device,
        hw_protocol::access_mode mode
    );

    /// Platform-specific: close hardware device
    void close_device(int fd);

    config::HardwareConfig resources_;
    std::unordered_map<hw_protocol::hw_handle, HardwareSession> sessions_;
    hw_protocol::hw_handle next_handle_{1};
    std::chrono::steady_clock::time_point last_timeout_check_;
    std::vector<messaging::Message> outbound_;
};

} // namespace protoflow::mainapp
