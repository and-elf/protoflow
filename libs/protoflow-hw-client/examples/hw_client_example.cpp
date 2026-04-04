// Example: Serial port communication using hardware arbitration client
//
// This example demonstrates how a registered application can use the
// hardware arbitration client to safely access a serial port through
// the main application's hardware arbitration service.

#include <protoflow/hw/hw.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

using namespace protoflow;

// Simple mock transport for demonstration
class example_transport : public rpc::transport_interface {
public:
    bool send(std::span<const std::byte> data) override {
        // In real implementation: send over TCP socket
        std::cout << "[Transport] Sending " << data.size() << " bytes\n";
        return true;
    }
    
    std::expected<std::vector<std::byte>, std::string> receive(size_t max_size) override {
        // In real implementation: receive from TCP socket
        return std::unexpected("Not implemented");
    }
    
    void close() override {
        std::cout << "[Transport] Closing connection\n";
    }
    
    bool is_connected() const override {
        return true;
    }
};

// Example 1: Basic serial port access
void example_basic_serial_access() {
    std::cout << "\n=== Example 1: Basic Serial Port Access ===\n";
    
    example_transport transport;
    hw::hw_client client{transport};
    
    // Request exclusive access to serial port with 30 second timeout
    auto access = client.request_access(
        "/dev/ttyUSB0",
        hw::protocol::access_mode::exclusive,
        std::chrono::seconds{30}
    );
    
    if (!access) {
        std::cerr << "Failed to access serial port: " 
                  << hw::to_string(access.error()) << '\n';
        return;
    }
    
    std::cout << "Hardware access granted!\n";
    std::cout << "  Handle: " << access->handle.id() << '\n';
    std::cout << "  Timeout: " << access->timeout.count() << " ms\n";
    std::cout << "  Capabilities: 0x" << std::hex << access->capabilities << std::dec << '\n';
    
    // Use RAII for automatic cleanup
    hw::scoped_hw_access scoped{client, access->handle};
    
    // Send command
    std::string cmd = "AT\r\n";
    std::vector<std::byte> data;
    for (char c : cmd) {
        data.push_back(static_cast<std::byte>(c));
    }
    
    auto write_result = client.write(scoped.handle(), data);
    if (!write_result) {
        std::cerr << "Write failed: " << hw::to_string(write_result.error()) << '\n';
        return;
    }
    
    std::cout << "Wrote " << *write_result << " bytes\n";
    
    // Read response
    auto read_result = client.read(scoped.handle(), 256);
    if (!read_result) {
        std::cerr << "Read failed: " << hw::to_string(read_result.error()) << '\n';
        return;
    }
    
    std::cout << "Read " << read_result->size() << " bytes\n";
    
    // Hardware automatically released when scoped goes out of scope
}

// Example 2: Manual resource management
void example_manual_resource_management() {
    std::cout << "\n=== Example 2: Manual Resource Management ===\n";
    
    example_transport transport;
    hw::hw_client client{transport};
    
    // Request access
    auto access = client.request_access("/dev/i2c-1");
    
    if (!access) {
        std::cerr << "Access denied: " << hw::to_string(access.error()) << '\n';
        return;
    }
    
    hw::hw_handle handle = access->handle;
    
    // Perform operations...
    std::vector<std::byte> i2c_data = {
        std::byte{0x50},  // Device address
        std::byte{0x00}   // Register address
    };
    
    auto write_result = client.write(handle, i2c_data);
    if (write_result) {
        std::cout << "I2C write successful\n";
    }
    
    // Manually release when done
    auto release_result = client.release(handle);
    if (release_result) {
        std::cout << "Hardware released successfully\n";
    } else {
        std::cerr << "Release failed: " << hw::to_string(release_result.error()) << '\n';
    }
}

// Example 3: Error handling
void example_error_handling() {
    std::cout << "\n=== Example 3: Error Handling ===\n";
    
    example_transport transport;
    hw::hw_client client{transport};
    
    // Try to access non-existent resource
    auto access = client.request_access("/dev/nonexistent");
    
    if (!access) {
        // Handle different error cases
        switch (access.error()) {
            case hw::hw_error::resource_not_found:
                std::cerr << "Hardware resource not found\n";
                break;
            case hw::hw_error::resource_busy:
                std::cerr << "Hardware resource is busy\n";
                break;
            case hw::hw_error::permission_denied:
                std::cerr << "Permission denied for hardware access\n";
                break;
            case hw::hw_error::timeout:
                std::cerr << "Hardware access timeout\n";
                break;
            default:
                std::cerr << "Unknown error: " << hw::to_string(access.error()) << '\n';
                break;
        }
        return;
    }
    
    // If we got access, use it
    // ...
}

// Example 4: Shared access mode
void example_shared_access() {
    std::cout << "\n=== Example 4: Shared Access Mode ===\n";
    
    example_transport transport;
    hw::hw_client client{transport};
    
    // Request shared access (multiple clients can read simultaneously)
    auto access = client.request_access(
        "/dev/temperature_sensor",
        hw::protocol::access_mode::shared,
        std::chrono::seconds{10}
    );
    
    if (!access) {
        std::cerr << "Failed to get shared access\n";
        return;
    }
    
    std::cout << "Shared access granted\n";
    
    // Multiple apps can now read from this sensor
    hw::scoped_hw_access scoped{client, access->handle};
    auto data = client.read(scoped.handle(), 4);
    
    if (data) {
        std::cout << "Read temperature sensor: " << data->size() << " bytes\n";
    }
}

// Example 5: IOCTL operations
void example_ioctl() {
    std::cout << "\n=== Example 5: IOCTL Operations ===\n";
    
    example_transport transport;
    hw::hw_client client{transport};
    
    auto access = client.request_access("/dev/custom_device");
    
    if (!access) {
        return;
    }
    
    hw::scoped_hw_access scoped{client, access->handle};
    
    // Check capabilities
    bool supports_ioctl = access->capabilities & 
                         static_cast<uint32_t>(hw::protocol::capability::ioctl);
    
    if (!supports_ioctl) {
        std::cerr << "Device does not support ioctl operations\n";
        return;
    }
    
    // Prepare ioctl argument
    std::vector<std::byte> arg = {
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04}
    };
    
    // Perform ioctl
    constexpr uint32_t CUSTOM_IOCTL_CMD = 0x12345678;
    auto result = client.ioctl(scoped.handle(), CUSTOM_IOCTL_CMD, arg);
    
    if (result) {
        std::cout << "IOCTL successful, response size: " << result->size() << '\n';
    } else {
        std::cerr << "IOCTL failed: " << hw::to_string(result.error()) << '\n';
    }
}

// Example 6: Checking resource capabilities
void example_check_capabilities() {
    std::cout << "\n=== Example 6: Check Resource Capabilities ===\n";
    
    example_transport transport;
    hw::hw_client client{transport};
    
    auto access = client.request_access("/dev/sda");
    
    if (!access) {
        return;
    }
    
    // Check what operations are supported
    uint32_t caps = access->capabilities;
    
    bool can_read = caps & static_cast<uint32_t>(hw::protocol::capability::read);
    bool can_write = caps & static_cast<uint32_t>(hw::protocol::capability::write);
    bool can_ioctl = caps & static_cast<uint32_t>(hw::protocol::capability::ioctl);
    bool is_seekable = caps & static_cast<uint32_t>(hw::protocol::capability::seekable);
    
    std::cout << "Hardware capabilities:\n";
    std::cout << "  Read:     " << (can_read ? "yes" : "no") << '\n';
    std::cout << "  Write:    " << (can_write ? "yes" : "no") << '\n';
    std::cout << "  IOCTL:    " << (can_ioctl ? "yes" : "no") << '\n';
    std::cout << "  Seekable: " << (is_seekable ? "yes" : "no") << '\n';
    
    hw::scoped_hw_access scoped{client, access->handle};
    
    // Only perform operations the device supports
    if (can_read && is_seekable) {
        // Read from specific offset
        auto data = client.read(scoped.handle(), 512, 0);  // offset=0
        if (data) {
            std::cout << "Read from offset 0: " << data->size() << " bytes\n";
        }
    }
}

int main() {
    std::cout << "Hardware Arbitration Client Examples\n";
    std::cout << "=====================================\n";
    
    // Note: These examples use a mock transport and will not actually
    // communicate with hardware. In a real application, you would:
    // 1. Connect to the main app via TCP
    // 2. Perform RPC handshake
    // 3. Use the hardware client with a real transport
    
    example_basic_serial_access();
    example_manual_resource_management();
    example_error_handling();
    example_shared_access();
    example_ioctl();
    example_check_capabilities();
    
    std::cout << "\n=== Examples Complete ===\n";
    
    return 0;
}
