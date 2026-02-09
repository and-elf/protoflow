#include <gtest/gtest.h>
#include <protoflow/messaging/message.hpp>

using namespace protoflow::messaging;

TEST(MessageHeaderTest, DefaultConstruction) {
    MessageHeader header;
    EXPECT_EQ(header.id, 0u);
    EXPECT_EQ(header.source, 0u);
    EXPECT_EQ(header.destination, 0u);
    EXPECT_EQ(header.priority, Priority::Normal);
    EXPECT_EQ(header.timestamp, 0u);
}

TEST(MessageHeaderTest, Equality) {
    MessageHeader h1{1, 10, 20, Priority::High, 1000};
    MessageHeader h2{1, 10, 20, Priority::High, 1000};
    MessageHeader h3{2, 10, 20, Priority::High, 1000};
    
    EXPECT_EQ(h1, h2);
    EXPECT_NE(h1, h3);
}

TEST(MessageTest, DefaultConstruction) {
    Message msg;
    EXPECT_EQ(msg.header.id, 0u);
    EXPECT_TRUE(msg.payload.empty());
    EXPECT_EQ(msg.size(), 0u);
}

TEST(MessageTest, ConstructionWithData) {
    MessageHeader header{1, 10, 20, Priority::Normal, 1000};
    std::vector<std::byte> data{std::byte{1}, std::byte{2}, std::byte{3}};
    
    Message msg{header, std::move(data)};
    
    EXPECT_EQ(msg.header, header);
    EXPECT_EQ(msg.size(), 3u);
    EXPECT_EQ(msg.data().size(), 3u);
}

TEST(MessageTest, MoveSemantics) {
    MessageHeader header{1, 10, 20, Priority::Normal, 1000};
    std::vector<std::byte> data{std::byte{1}, std::byte{2}, std::byte{3}};
    
    Message msg1{header, std::move(data)};
    Message msg2 = std::move(msg1);
    
    EXPECT_EQ(msg2.header, header);
    EXPECT_EQ(msg2.size(), 3u);
}

TEST(MessageTest, DataSpan) {
    std::vector<std::byte> data{std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}};
    Message msg{MessageHeader{}, std::vector(data)};
    
    auto span = msg.data();
    EXPECT_EQ(span.size(), 3u);
    EXPECT_EQ(span[0], std::byte{0xAA});
    EXPECT_EQ(span[1], std::byte{0xBB});
    EXPECT_EQ(span[2], std::byte{0xCC});
}

TEST(MessageBuilderTest, BuildEmptyMessage) {
    auto msg = MessageBuilder().build();
    
    EXPECT_EQ(msg.header.id, 0u);
    EXPECT_EQ(msg.header.source, 0u);
    EXPECT_EQ(msg.header.destination, 0u);
    EXPECT_EQ(msg.header.priority, Priority::Normal);
    EXPECT_TRUE(msg.payload.empty());
}

TEST(MessageBuilderTest, BuildCompleteMessage) {
    std::vector<std::byte> data{std::byte{1}, std::byte{2}};
    
    auto msg = MessageBuilder()
        .id(42)
        .from(100)
        .to(200)
        .priority(Priority::High)
        .timestamp(9999)
        .payload(std::move(data))
        .build();
    
    EXPECT_EQ(msg.header.id, 42u);
    EXPECT_EQ(msg.header.source, 100u);
    EXPECT_EQ(msg.header.destination, 200u);
    EXPECT_EQ(msg.header.priority, Priority::High);
    EXPECT_EQ(msg.header.timestamp, 9999u);
    EXPECT_EQ(msg.size(), 2u);
}

TEST(MessageBuilderTest, FluentInterface) {
    auto msg = MessageBuilder()
        .id(1)
        .from(2)
        .to(3)
        .build();
    
    EXPECT_EQ(msg.header.id, 1u);
    EXPECT_EQ(msg.header.source, 2u);
    EXPECT_EQ(msg.header.destination, 3u);
}

TEST(MessageBuilderTest, PayloadFromSpan) {
    std::vector<std::byte> data{std::byte{0x11}, std::byte{0x22}, std::byte{0x33}};
    std::span<const std::byte> span(data);
    
    auto msg = MessageBuilder()
        .payload(span)
        .build();
    
    EXPECT_EQ(msg.size(), 3u);
    EXPECT_EQ(msg.data()[0], std::byte{0x11});
    EXPECT_EQ(msg.data()[1], std::byte{0x22});
    EXPECT_EQ(msg.data()[2], std::byte{0x33});
}

TEST(MessageBuilderTest, OverwriteValues) {
    auto msg = MessageBuilder()
        .id(1)
        .id(2)  // Overwrite
        .from(10)
        .from(20)  // Overwrite
        .build();
    
    EXPECT_EQ(msg.header.id, 2u);
    EXPECT_EQ(msg.header.source, 20u);
}

TEST(PriorityTest, AllLevels) {
    EXPECT_EQ(static_cast<uint8_t>(Priority::Low), 0);
    EXPECT_EQ(static_cast<uint8_t>(Priority::Normal), 1);
    EXPECT_EQ(static_cast<uint8_t>(Priority::High), 2);
    EXPECT_EQ(static_cast<uint8_t>(Priority::Critical), 3);
}
