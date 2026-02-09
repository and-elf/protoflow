#include <protoflow/hw/hw_client.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <gtest/gtest.h>
#include <vector>
#include <queue>
#include <cstring>

using namespace protoflow::hw;
namespace rpc_proto = protoflow::rpc::protocol;
namespace hw_proto = protoflow::hw::protocol;

// Mock transport for testing
class mock_transport : public protoflow::rpc::transport_interface {
public:
    bool send(std::span<const std::byte> data) override {
        sent_data.insert(sent_data.end(), data.begin(), data.end());
        return send_success;
    }
    
    std::expected<std::vector<std::byte>, std::string> receive(size_t max_size) override {
        if (!receive_success) {
            return std::unexpected("Mock receive error");
        }
        
        if (receive_queue.empty()) {
            return std::unexpected("No data to receive");
        }
        
        auto data = std::move(receive_queue.front());
        receive_queue.pop();
        
        if (data.size() > max_size) {
            data.resize(max_size);
        }
        
        return data;
    }
    
    void close() override {
        is_open = false;
    }
    
    bool is_connected() const override {
        return is_open;
    }
    
    // Test helpers
    void queue_response(std::vector<std::byte> data) {
        receive_queue.push(std::move(data));
    }
    
    template<typename T>
    void queue_message(hw_proto::cmd command, const T& msg) {
        // Queue header first
        auto header = rpc_proto::make_header(
            static_cast<rpc_proto::cmd>(command),
            sizeof(T)
        );
        
        std::vector<std::byte> header_data(rpc_proto::rpc_header::wire_size);
        std::memcpy(header_data.data(), &header, rpc_proto::rpc_header::wire_size);
        receive_queue.push(std::move(header_data));
        
        // Queue payload separately
        std::vector<std::byte> payload_data(sizeof(T));
        std::memcpy(payload_data.data(), &msg, sizeof(T));
        receive_queue.push(std::move(payload_data));
    }
    
    std::vector<std::byte> sent_data;
    std::queue<std::vector<std::byte>> receive_queue;
    bool send_success = true;
    bool receive_success = true;
    bool is_open = true;
};

// Test hw_handle
TEST(HwClientTest, HwHandle) {
    hw_handle h1{42};
    EXPECT_EQ(h1.id(), 42u);
    EXPECT_TRUE(h1.is_valid());
    EXPECT_TRUE(static_cast<bool>(h1));
    
    hw_handle h2{0};
    EXPECT_EQ(h2.id(), 0u);
    EXPECT_FALSE(h2.is_valid());
    EXPECT_FALSE(static_cast<bool>(h2));
}

// Test error to string conversion
TEST(HwClientTest, ErrorToString) {
    EXPECT_EQ(to_string(hw_error::transport_error), "Transport error");
    EXPECT_EQ(to_string(hw_error::resource_not_found), "Resource not found");
    EXPECT_EQ(to_string(hw_error::permission_denied), "Permission denied");
}

// Test successful hardware access request
TEST(HwClientTest, RequestAccessSuccess) {
    mock_transport transport;
    hw_client client{transport};
    
    // Queue success response
    hw_proto::hw_access_granted_msg response{};
    response.handle = 123;
    response.timeout_ms = 5000;
    response.capabilities = static_cast<uint32_t>(hw_proto::capability::read) |
                           static_cast<uint32_t>(hw_proto::capability::write);
    response.reserved = 0;
    
    transport.queue_message(hw_proto::cmd::hw_access_granted, response);
    
    // Request access
    auto result = client.request_access("/dev/ttyUSB0");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->handle.id(), 123u);
    EXPECT_EQ(result->timeout.count(), 5000);
    EXPECT_EQ(result->capabilities, response.capabilities);
}

// Test hardware access denied
TEST(HwClientTest, RequestAccessDenied) {
    mock_transport transport;
    hw_client client{transport};
    
    // Queue denied response
    hw_proto::hw_access_denied_msg response{};
    response.reason_code = static_cast<uint32_t>(hw_proto::error_code::resource_busy);
    std::strncpy(response.reason, "Resource is busy", sizeof(response.reason) - 1);
    
    transport.queue_message(hw_proto::cmd::hw_access_denied, response);
    
    // Request access
    auto result = client.request_access("/dev/ttyUSB0");
    
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), hw_error::resource_busy);
}

// Test hardware release
TEST(HwClientTest, ReleaseSuccess) {
    mock_transport transport;
    hw_client client{transport};
    
    // Queue release ack
    hw_proto::hw_release_ack_msg response{};
    response.handle = 123;
    response.status = 0;
    
    transport.queue_message(hw_proto::cmd::hw_release_ack, response);
    
    // Release handle
    hw_handle handle{123};
    auto result = client.release(handle);
    
    ASSERT_TRUE(result.has_value());
}

// Test invalid handle release
TEST(HwClientTest, ReleaseInvalidHandle) {
    mock_transport transport;
    hw_client client{transport};
    
    hw_handle invalid{0};
    auto result = client.release(invalid);
    
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), hw_error::invalid_handle);
}

// Test hardware write
TEST(HwClientTest, WriteSuccess) {
    mock_transport transport;
    hw_client client{transport};
    
    // Queue write ack
    hw_proto::hw_write_ack_msg response{};
    response.handle = 123;
    response.bytes_written = 10;
    response.status = 0;
    response.reserved = 0;
    
    transport.queue_message(hw_proto::cmd::hw_write_ack, response);
    
    // Write data
    hw_handle handle{123};
    std::vector<std::byte> data(10, std::byte{0x42});
    auto result = client.write(handle, data);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 10u);
}

// Test hardware read
TEST(HwClientTest, ReadSuccess) {
    mock_transport transport;
    hw_client client{transport};
    
    // Prepare response with data
    hw_proto::hw_read_response_msg response{};
    response.handle = 123;
    response.bytes_read = 5;
    response.status = 0;
    response.reserved = 0;
    
    auto header = rpc_proto::make_header(
        static_cast<rpc_proto::cmd>(hw_proto::cmd::hw_read_response),
        hw_proto::hw_read_response_msg::wire_size + 5  // response struct + 5 bytes of data
    );
    
    // Queue header
    std::vector<std::byte> header_data(rpc_proto::rpc_header::wire_size);
    std::memcpy(header_data.data(), &header, rpc_proto::rpc_header::wire_size);
    transport.queue_response(std::move(header_data));
    
    // Queue payload (response struct + data)
    std::vector<std::byte> payload_data;
    payload_data.resize(hw_proto::hw_read_response_msg::wire_size + 5);
    std::memcpy(payload_data.data(), &response, hw_proto::hw_read_response_msg::wire_size);
    
    // Fill with test data
    for (size_t i = 0; i < 5; ++i) {
        payload_data[hw_proto::hw_read_response_msg::wire_size + i] = std::byte{0x55};
    }
    
    transport.queue_response(std::move(payload_data));
    
    // Read data
    hw_handle handle{123};
    auto result = client.read(handle, 256);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 5u);
    
    for (const auto& byte : *result) {
        EXPECT_EQ(byte, std::byte{0x55});
    }
}

// Test scoped_hw_access RAII
TEST(HwClientTest, ScopedHwAccess) {
    mock_transport transport;
    hw_client client{transport};
    
    // Queue release ack for RAII destructor
    hw_proto::hw_release_ack_msg response{};
    response.handle = 123;
    response.status = 0;
    
    transport.queue_message(hw_proto::cmd::hw_release_ack, response);
    
    {
        hw_handle handle{123};
        scoped_hw_access scoped{client, handle};
        
        EXPECT_EQ(scoped.handle().id(), 123u);
    }
    
    // Destructor should have sent release request
    EXPECT_FALSE(transport.sent_data.empty());
}

// Test transport error handling
TEST(HwClientTest, TransportError) {
    mock_transport transport;
    hw_client client{transport};
    
    transport.send_success = false;
    
    auto result = client.request_access("/dev/ttyUSB0");
    
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), hw_error::transport_error);
}

// Test buffer too large error
TEST(HwClientTest, BufferTooLarge) {
    mock_transport transport;
    hw_client client{transport};
    
    hw_handle handle{123};
    
    // Try to write more than max_io_size (1 MB)
    std::vector<std::byte> large_data(2 * 1024 * 1024, std::byte{0});
    auto result = client.write(handle, large_data);
    
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), hw_error::buffer_too_large);
}
