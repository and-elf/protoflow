#pragma once

#include <protoflow/service.hpp>
#include <protoflow/fsm.hpp>
#include <protoflow/hw/protocol.hpp>
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
    hw::hw_handle handle;
    std::string app_name;
    std::string resource;
    hw::access_mode mode;
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

    /// Request hardware access
    [[nodiscard]] std::expected<hw::hw_handle, hw::error_code> request_access(
        const std::string& app_name,
        const std::string& resource,
        hw::access_mode mode,
        std::chrono::seconds timeout
    );

    /// Release hardware access
    [[nodiscard]] hw::error_code release_access(hw::hw_handle handle);

    /// Read from hardware
    [[nodiscard]] std::expected<std::vector<std::byte>, hw::error_code> read(
        hw::hw_handle handle,
        size_t max_length
    );

    /// Write to hardware
    [[nodiscard]] std::expected<size_t, hw::error_code> write(
        hw::hw_handle handle,
        std::span<const std::byte> data
    );

    /// Perform IOCTL operation
    [[nodiscard]] std::expected<hw::ioctl_result, hw::error_code> ioctl(
        hw::hw_handle handle,
        uint32_t request,
        std::span<const std::byte> args
    );

protected:
    void handle(messaging::Message&& msg) override;
    std::vector<messaging::Message> generate_outbound() override;

private:
    /// Check for expired sessions
    void check_timeouts();

    /// Validate handle
    [[nodiscard]] HardwareSession* get_session(hw::hw_handle handle);

    /// Check if resource can be acquired
    [[nodiscard]] bool can_acquire(
        const std::string& resource,
        hw::access_mode mode
    ) const;

    /// Platform-specific: open hardware device
    [[nodiscard]] std::expected<int, hw::error_code> open_device(
        const std::string& device,
        hw::access_mode mode
    );

    /// Platform-specific: close hardware device
    void close_device(int fd);

    config::HardwareConfig resources_;
    std::unordered_map<hw::hw_handle, HardwareSession> sessions_;
    hw::hw_handle next_handle_{1};
    std::chrono::steady_clock::time_point last_timeout_check_;
};

} // namespace protoflow::mainapp
