#include <protoflow/hw/protocol.hpp>
#include <gtest/gtest.h>

using namespace protoflow::hw::protocol;

// Test message sizes
TEST(HwProtocolTest, MessageSizes) {
    EXPECT_EQ(sizeof(request_hw_access_msg), request_hw_access_msg::wire_size);
    EXPECT_EQ(sizeof(hw_access_granted_msg), hw_access_granted_msg::wire_size);
    EXPECT_EQ(sizeof(hw_access_denied_msg), hw_access_denied_msg::wire_size);
    EXPECT_EQ(sizeof(hw_release_msg), hw_release_msg::wire_size);
    EXPECT_EQ(sizeof(hw_release_ack_msg), hw_release_ack_msg::wire_size);
    EXPECT_EQ(sizeof(hw_write_msg), hw_write_msg::wire_size);
    EXPECT_EQ(sizeof(hw_write_ack_msg), hw_write_ack_msg::wire_size);
    EXPECT_EQ(sizeof(hw_read_msg), hw_read_msg::wire_size);
    EXPECT_EQ(sizeof(hw_read_response_msg), hw_read_response_msg::wire_size);
    EXPECT_EQ(sizeof(hw_ioctl_msg), hw_ioctl_msg::wire_size);
    EXPECT_EQ(sizeof(hw_ioctl_response_msg), hw_ioctl_response_msg::wire_size);
    EXPECT_EQ(sizeof(hw_error_msg), hw_error_msg::wire_size);
    EXPECT_EQ(sizeof(hw_timeout_msg), hw_timeout_msg::wire_size);
}

// Test request_hw_access_msg initialization
TEST(HwProtocolTest, RequestHwAccessMsg) {
    request_hw_access_msg msg{};
    
    std::strncpy(msg.resource, "/dev/ttyUSB0", sizeof(msg.resource) - 1);
    msg.mode = static_cast<uint32_t>(access_mode::exclusive);
    msg.timeout_ms = 5000;
    msg.flags = 0;
    
    EXPECT_STREQ(msg.resource, "/dev/ttyUSB0");
    EXPECT_EQ(msg.mode, static_cast<uint32_t>(access_mode::exclusive));
    EXPECT_EQ(msg.timeout_ms, 5000u);
    EXPECT_EQ(msg.flags, 0u);
}

// Test hw_access_granted_msg
TEST(HwProtocolTest, HwAccessGrantedMsg) {
    hw_access_granted_msg msg{};
    
    msg.handle = 42;
    msg.timeout_ms = 5000;
    msg.capabilities = static_cast<uint32_t>(capability::read) | 
                      static_cast<uint32_t>(capability::write);
    msg.reserved = 0;
    
    EXPECT_EQ(msg.handle, 42u);
    EXPECT_EQ(msg.timeout_ms, 5000u);
    EXPECT_EQ(msg.capabilities & static_cast<uint32_t>(capability::read), 
              static_cast<uint32_t>(capability::read));
    EXPECT_EQ(msg.capabilities & static_cast<uint32_t>(capability::write), 
              static_cast<uint32_t>(capability::write));
}

// Test hw_write_msg
TEST(HwProtocolTest, HwWriteMsg) {
    hw_write_msg msg{};
    
    msg.handle = 42;
    msg.offset = 0;
    msg.length = 256;
    msg.flags = 0;
    
    EXPECT_EQ(msg.handle, 42u);
    EXPECT_EQ(msg.offset, 0u);
    EXPECT_EQ(msg.length, 256u);
}

// Test hw_read_msg
TEST(HwProtocolTest, HwReadMsg) {
    hw_read_msg msg{};
    
    msg.handle = 42;
    msg.offset = 0;
    msg.length = 256;
    msg.flags = 0;
    
    EXPECT_EQ(msg.handle, 42u);
    EXPECT_EQ(msg.length, 256u);
}

// Test error codes
TEST(HwProtocolTest, ErrorCodes) {
    EXPECT_EQ(static_cast<uint32_t>(error_code::success), 0u);
    EXPECT_NE(static_cast<uint32_t>(error_code::resource_not_found), 0u);
    EXPECT_NE(static_cast<uint32_t>(error_code::resource_busy), 0u);
    EXPECT_NE(static_cast<uint32_t>(error_code::permission_denied), 0u);
}

// Test capability flags
TEST(HwProtocolTest, CapabilityFlags) {
    uint32_t caps = static_cast<uint32_t>(capability::read) | 
                    static_cast<uint32_t>(capability::write) |
                    static_cast<uint32_t>(capability::ioctl);
    
    EXPECT_NE(caps & static_cast<uint32_t>(capability::read), 0u);
    EXPECT_NE(caps & static_cast<uint32_t>(capability::write), 0u);
    EXPECT_NE(caps & static_cast<uint32_t>(capability::ioctl), 0u);
    EXPECT_EQ(caps & static_cast<uint32_t>(capability::seekable), 0u);
}

// Test access modes
TEST(HwProtocolTest, AccessModes) {
    EXPECT_EQ(static_cast<uint32_t>(access_mode::exclusive), 0u);
    EXPECT_EQ(static_cast<uint32_t>(access_mode::shared), 1u);
}
