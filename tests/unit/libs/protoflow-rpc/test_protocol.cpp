#include <protoflow/rpc/protocol.hpp>
#include <gtest/gtest.h>
#include <cstring>

using namespace protoflow::rpc;
using namespace protoflow::rpc::protocol;

class ProtocolTest : public ::testing::Test {};

// Test protocol constants
TEST_F(ProtocolTest, WireVersion) {
    EXPECT_EQ(wire_version, 1u);
}

TEST_F(ProtocolTest, MagicNumber) {
    EXPECT_EQ(magic, 0x30435052u); // 'RPC0'
}

// Test header structure
TEST_F(ProtocolTest, HeaderSize) {
    EXPECT_EQ(sizeof(rpc_header), 16u);
    EXPECT_EQ(rpc_header::wire_size, 16u);
}

TEST_F(ProtocolTest, HeaderConstruction) {
    rpc_header hdr{
        .magic = magic,
        .version = wire_version,
        .cmd = static_cast<uint16_t>(cmd::hello),
        .reserved = 0,
        .payload_size = 100
    };

    EXPECT_EQ(hdr.magic, magic);
    EXPECT_EQ(hdr.version, wire_version);
    EXPECT_EQ(hdr.cmd, static_cast<uint16_t>(cmd::hello));
    EXPECT_EQ(hdr.reserved, 0u);
    EXPECT_EQ(hdr.payload_size, 100u);
}

TEST_F(ProtocolTest, HeaderValidation) {
    rpc_header valid_hdr{
        .magic = magic,
        .version = wire_version,
        .cmd = 1,
        .reserved = 0,
        .payload_size = 0
    };

    EXPECT_TRUE(valid_hdr.is_valid());
    EXPECT_TRUE(valid_hdr.version_compatible());

    rpc_header invalid_magic{
        .magic = 0xDEADBEEF,
        .version = wire_version,
        .cmd = 1,
        .reserved = 0,
        .payload_size = 0
    };

    EXPECT_FALSE(invalid_magic.is_valid());

    rpc_header invalid_version{
        .magic = magic,
        .version = 999,
        .cmd = 1,
        .reserved = 0,
        .payload_size = 0
    };

    EXPECT_TRUE(invalid_version.is_valid());
    EXPECT_FALSE(invalid_version.version_compatible());
}

TEST_F(ProtocolTest, MakeHeader) {
    auto hdr = make_header(cmd::hello_ack, 256);

    EXPECT_EQ(hdr.magic, magic);
    EXPECT_EQ(hdr.version, wire_version);
    EXPECT_EQ(hdr.cmd, static_cast<uint16_t>(cmd::hello_ack));
    EXPECT_EQ(hdr.reserved, 0u);
    EXPECT_EQ(hdr.payload_size, 256u);
    EXPECT_TRUE(hdr.is_valid());
    EXPECT_TRUE(hdr.version_compatible());
}

// Test command enum values
TEST_F(ProtocolTest, CommandValues) {
    EXPECT_EQ(static_cast<uint16_t>(cmd::hello), 1);
    EXPECT_EQ(static_cast<uint16_t>(cmd::hello_ack), 2);
    EXPECT_EQ(static_cast<uint16_t>(cmd::register_app), 3);
    EXPECT_EQ(static_cast<uint16_t>(cmd::register_ack), 4);
    EXPECT_EQ(static_cast<uint16_t>(cmd::heartbeat), 5);
    EXPECT_EQ(static_cast<uint16_t>(cmd::heartbeat_ack), 6);
    EXPECT_EQ(static_cast<uint16_t>(cmd::render_fragment), 7);
    EXPECT_EQ(static_cast<uint16_t>(cmd::fragment_data), 8);
    EXPECT_EQ(static_cast<uint16_t>(cmd::get_state), 9);
    EXPECT_EQ(static_cast<uint16_t>(cmd::state_json), 10);
    EXPECT_EQ(static_cast<uint16_t>(cmd::error), 255);
}

// Test message structures
TEST_F(ProtocolTest, HelloMessageSize) {
    EXPECT_EQ(sizeof(hello_msg), 4u);
    EXPECT_EQ(hello_msg::wire_size, 4u);
}

TEST_F(ProtocolTest, HelloAckSize) {
    EXPECT_EQ(sizeof(hello_ack), 4u);
    EXPECT_EQ(hello_ack::wire_size, 4u);
}

TEST_F(ProtocolTest, RegisterAppSize) {
    EXPECT_EQ(sizeof(register_app_msg), 68u);
    EXPECT_EQ(register_app_msg::wire_size, 68u);
}

TEST_F(ProtocolTest, RegisterAckSize) {
    EXPECT_EQ(sizeof(register_ack), 8u);
    EXPECT_EQ(register_ack::wire_size, 8u);
}

TEST_F(ProtocolTest, HeartbeatSize) {
    EXPECT_EQ(sizeof(heartbeat_msg), 8u);
    EXPECT_EQ(heartbeat_msg::wire_size, 8u);
}

TEST_F(ProtocolTest, HeartbeatAckSize) {
    EXPECT_EQ(sizeof(heartbeat_ack), 8u);
    EXPECT_EQ(heartbeat_ack::wire_size, 8u);
}

TEST_F(ProtocolTest, RenderFragmentSize) {
    EXPECT_EQ(sizeof(render_fragment_msg), 128u);
    EXPECT_EQ(render_fragment_msg::wire_size, 128u);
}

TEST_F(ProtocolTest, GetStateSize) {
    EXPECT_EQ(sizeof(get_state_msg), 4u);
    EXPECT_EQ(get_state_msg::wire_size, 4u);
}

TEST_F(ProtocolTest, ErrorMessageSize) {
    EXPECT_EQ(sizeof(error_msg), 260u);
    EXPECT_EQ(error_msg::wire_size, 260u);
}

// Test message content
TEST_F(ProtocolTest, HelloMessageContent) {
    hello_msg msg{42};
    EXPECT_EQ(msg.version, 42u);
}

TEST_F(ProtocolTest, RegisterAppContent) {
    register_app_msg msg{};
    std::strcpy(msg.name, "test-app");
    msg.endpoint_count = 3;

    EXPECT_STREQ(msg.name, "test-app");
    EXPECT_EQ(msg.endpoint_count, 3u);
}

TEST_F(ProtocolTest, RegisterAckContent) {
    register_ack ack{123, 0};
    EXPECT_EQ(ack.app_id, 123u);
    EXPECT_EQ(ack.status, 0u);
}

TEST_F(ProtocolTest, HeartbeatContent) {
    heartbeat_msg msg{1234567890};
    EXPECT_EQ(msg.timestamp, 1234567890u);
}

TEST_F(ProtocolTest, RenderFragmentContent) {
    render_fragment_msg msg{};
    std::strcpy(msg.fragment_id, "dashboard");
    EXPECT_STREQ(msg.fragment_id, "dashboard");
}

TEST_F(ProtocolTest, ErrorMessageContent) {
    error_msg err{};
    err.error_code = 42;
    std::strcpy(err.message, "Something went wrong");

    EXPECT_EQ(err.error_code, 42u);
    EXPECT_STREQ(err.message, "Something went wrong");
}
