#include <protoflow/rpc/rpc_base.hpp>
#include <gtest/gtest.h>
#include <vector>
#include <cstring>

using namespace protoflow::rpc;

// Mock transport for testing
class mock_transport : public transport_interface {
public:
    std::vector<std::byte> sent_data;
    std::vector<std::byte> receive_buffer;
    size_t receive_offset = 0;
    bool connected = true;
    bool should_fail_send = false;
    bool should_fail_receive = false;

    bool send(std::span<const std::byte> data) override {
        if (should_fail_send) {
            return false;
        }
        sent_data.insert(sent_data.end(), data.begin(), data.end());
        return true;
    }

    std::expected<std::vector<std::byte>, std::string> receive(size_t max_size) override {
        if (should_fail_receive) {
            return std::unexpected("Mock receive failure");
        }

        if (receive_offset >= receive_buffer.size()) {
            return std::unexpected("No data available");
        }

        size_t available = receive_buffer.size() - receive_offset;
        size_t to_read = std::min(available, max_size);

        std::vector<std::byte> result(
            receive_buffer.begin() + static_cast<std::ptrdiff_t>(receive_offset),
            receive_buffer.begin() + static_cast<std::ptrdiff_t>(receive_offset + to_read)
        );

        receive_offset += to_read;
        return result;
    }

    void close() override {
        connected = false;
    }

    bool is_connected() const override {
        return connected;
    }

    void reset() {
        sent_data.clear();
        receive_buffer.clear();
        receive_offset = 0;
        connected = true;
        should_fail_send = false;
        should_fail_receive = false;
    }

    void add_receive_data(const std::vector<std::byte>& data) {
        receive_buffer.insert(receive_buffer.end(), data.begin(), data.end());
    }
};

// Test fixture for server
class RpcServerTest : public ::testing::Test {
protected:
    rpc_server_base server;
    mock_transport transport;

    void SetUp() override {
        transport.reset();
    }
};

TEST_F(RpcServerTest, SendHelloAck) {
    EXPECT_TRUE(server.send_hello_ack(transport));

    ASSERT_GE(transport.sent_data.size(), rpc_header::wire_size);

    rpc_header hdr;
    std::memcpy(&hdr, transport.sent_data.data(), rpc_header::wire_size);

    EXPECT_TRUE(hdr.is_valid());
    EXPECT_TRUE(hdr.version_compatible());
    EXPECT_EQ(hdr.cmd, static_cast<uint16_t>(cmd::hello_ack));
    EXPECT_EQ(hdr.payload_size, hello_ack::wire_size);
}

TEST_F(RpcServerTest, HandleHelloSuccess) {
    hello_msg msg{wire_version};
    rpc_header hdr = make_header(cmd::hello, 
                                                      hello_msg::wire_size);

    auto payload = std::span{reinterpret_cast<const std::byte*>(&msg), 
                            hello_msg::wire_size};

    EXPECT_TRUE(server.handle_hello(hdr, payload, transport));
    EXPECT_TRUE(transport.is_connected());
}

TEST_F(RpcServerTest, HandleHelloVersionMismatch) {
    hello_msg msg{999}; // Wrong version
    rpc_header hdr{
        .magic = magic,
        .version = 999,
        .cmd = static_cast<uint16_t>(cmd::hello),
        .reserved = 0,
        .payload_size = hello_msg::wire_size
    };

    auto payload = std::span{reinterpret_cast<const std::byte*>(&msg),
                            hello_msg::wire_size};

    EXPECT_FALSE(server.handle_hello(hdr, payload, transport));
    EXPECT_FALSE(transport.is_connected());
}

TEST_F(RpcServerTest, HandleHelloInvalidPayloadSize) {
    rpc_header hdr = make_header(cmd::hello, 1);
    std::byte dummy[1] = {std::byte{0}};

    EXPECT_FALSE(server.handle_hello(hdr, std::span{dummy, 1}, transport));
}

TEST_F(RpcServerTest, SendError) {
    EXPECT_TRUE(server.send_error(transport, 42, "Test error message"));

    ASSERT_GE(transport.sent_data.size(), 
              rpc_header::wire_size + error_msg::wire_size);

    rpc_header hdr;
    std::memcpy(&hdr, transport.sent_data.data(), rpc_header::wire_size);

    EXPECT_EQ(hdr.cmd, static_cast<uint16_t>(cmd::error));
    EXPECT_EQ(hdr.payload_size, error_msg::wire_size);

    error_msg err;
    std::memcpy(&err, transport.sent_data.data() + rpc_header::wire_size,
                error_msg::wire_size);

    EXPECT_EQ(err.error_code, 42u);
    EXPECT_STREQ(err.message, "Test error message");
}

TEST_F(RpcServerTest, SendMessageSuccess) {
    std::vector<std::byte> payload = {std::byte{1}, std::byte{2}, std::byte{3}};
    
    EXPECT_TRUE(server.send_message(transport, cmd::heartbeat, payload));

    ASSERT_GE(transport.sent_data.size(), rpc_header::wire_size + 3);

    rpc_header hdr;
    std::memcpy(&hdr, transport.sent_data.data(), rpc_header::wire_size);

    EXPECT_TRUE(hdr.is_valid());
    EXPECT_EQ(hdr.cmd, static_cast<uint16_t>(cmd::heartbeat));
    EXPECT_EQ(hdr.payload_size, 3u);
}

TEST_F(RpcServerTest, SendMessagePayloadTooLarge) {
    std::vector<std::byte> huge_payload(rpc_server_base::max_payload_size + 1);
    
    EXPECT_FALSE(server.send_message(transport, cmd::heartbeat, huge_payload));
}

TEST_F(RpcServerTest, ReceiveHeaderSuccess) {
    auto hdr = make_header(cmd::hello, 100);
    auto* hdr_bytes = reinterpret_cast<std::byte*>(&hdr);
    transport.add_receive_data(std::vector<std::byte>(hdr_bytes, 
                                                       hdr_bytes + rpc_header::wire_size));

    auto result = server.receive_header(transport);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_valid());
    EXPECT_EQ(result->cmd, static_cast<uint16_t>(cmd::hello));
    EXPECT_EQ(result->payload_size, 100u);
}

TEST_F(RpcServerTest, ReceiveHeaderInvalidMagic) {
    rpc_header bad_hdr{
        .magic = 0xDEADBEEF,
        .version = wire_version,
        .cmd = 1,
        .reserved = 0,
        .payload_size = 0
    };

    auto* hdr_bytes = reinterpret_cast<std::byte*>(&bad_hdr);
    transport.add_receive_data(std::vector<std::byte>(hdr_bytes,
                                                       hdr_bytes + rpc_header::wire_size));

    auto result = server.receive_header(transport);

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Invalid magic"), std::string::npos);
}

TEST_F(RpcServerTest, ReceiveHeaderPayloadTooLarge) {
    auto hdr = make_header(cmd::hello, 
                                     rpc_server_base::max_payload_size + 1);
    auto* hdr_bytes = reinterpret_cast<std::byte*>(&hdr);
    transport.add_receive_data(std::vector<std::byte>(hdr_bytes,
                                                       hdr_bytes + rpc_header::wire_size));

    auto result = server.receive_header(transport);

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("too large"), std::string::npos);
}

TEST_F(RpcServerTest, ReceivePayloadSuccess) {
    std::vector<std::byte> payload = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    transport.add_receive_data(payload);

    auto result = server.receive_payload(transport, 4);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 4u);
    EXPECT_EQ((*result)[0], std::byte{1});
    EXPECT_EQ((*result)[3], std::byte{4});
}

TEST_F(RpcServerTest, ReceivePayloadZeroSize) {
    auto result = server.receive_payload(transport, 0);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 0u);
}

// Test fixture for client
class RpcClientTest : public ::testing::Test {
protected:
    rpc_client_base client;
    mock_transport transport;

    void SetUp() override {
        transport.reset();
    }
};

TEST_F(RpcClientTest, SendHello) {
    EXPECT_TRUE(client.send_hello(transport));

    ASSERT_GE(transport.sent_data.size(), 
              rpc_header::wire_size + hello_msg::wire_size);

    rpc_header hdr;
    std::memcpy(&hdr, transport.sent_data.data(), rpc_header::wire_size);

    EXPECT_TRUE(hdr.is_valid());
    EXPECT_EQ(hdr.cmd, static_cast<uint16_t>(cmd::hello));
    EXPECT_EQ(hdr.payload_size, hello_msg::wire_size);

    hello_msg msg;
    std::memcpy(&msg, transport.sent_data.data() + rpc_header::wire_size,
                hello_msg::wire_size);

    EXPECT_EQ(msg.version, wire_version);
}

TEST_F(RpcClientTest, SendHeartbeat) {
    uint64_t timestamp = 1234567890;
    EXPECT_TRUE(client.send_heartbeat(transport, timestamp));

    ASSERT_GE(transport.sent_data.size(),
              rpc_header::wire_size + heartbeat_msg::wire_size);

    rpc_header hdr;
    std::memcpy(&hdr, transport.sent_data.data(), rpc_header::wire_size);

    EXPECT_EQ(hdr.cmd, static_cast<uint16_t>(cmd::heartbeat));
    EXPECT_EQ(hdr.payload_size, heartbeat_msg::wire_size);

    heartbeat_msg msg;
    std::memcpy(&msg, transport.sent_data.data() + rpc_header::wire_size,
                heartbeat_msg::wire_size);

    EXPECT_EQ(msg.timestamp, timestamp);
}

TEST_F(RpcClientTest, ReceiveHeaderVersionMismatch) {
    rpc_header bad_hdr{
        .magic = magic,
        .version = 999,
        .cmd = 1,
        .reserved = 0,
        .payload_size = 0
    };

    auto* hdr_bytes = reinterpret_cast<std::byte*>(&bad_hdr);
    transport.add_receive_data(std::vector<std::byte>(hdr_bytes,
                                                       hdr_bytes + rpc_header::wire_size));

    auto result = client.receive_header(transport);

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("version"), std::string::npos);
}
