#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <span>

namespace protoflow::messaging {

/// Message ID type for routing
using MessageId = uint64_t;

/// Service ID type for addressing
using ServiceId = uint32_t;

/// Message priority levels
enum class Priority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/// Message header containing routing and metadata
struct MessageHeader {
    MessageId id{0};
    ServiceId source{0};
    ServiceId destination{0};
    Priority priority{Priority::Normal};
    uint64_t timestamp{0};
    
    bool operator==(const MessageHeader&) const = default;
};

/// Base message type
/// All messages must be movable and have a header
struct Message {
    MessageHeader header;
    std::vector<std::byte> payload;
    
    Message() = default;
    Message(MessageHeader hdr, std::vector<std::byte> data)
        : header(hdr), payload(std::move(data)) {}
    
    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;
    Message(Message&&) = default;
    Message& operator=(Message&&) = default;
    
    [[nodiscard]] std::span<const std::byte> data() const noexcept {
        return payload;
    }
    
    [[nodiscard]] std::size_t size() const noexcept {
        return payload.size();
    }
};

/// Message builder for constructing messages
class MessageBuilder {
public:
    MessageBuilder& id(MessageId msg_id) {
        header_.id = msg_id;
        return *this;
    }
    
    MessageBuilder& from(ServiceId src) {
        header_.source = src;
        return *this;
    }
    
    MessageBuilder& to(ServiceId dst) {
        header_.destination = dst;
        return *this;
    }
    
    MessageBuilder& priority(Priority p) {
        header_.priority = p;
        return *this;
    }
    
    MessageBuilder& timestamp(uint64_t ts) {
        header_.timestamp = ts;
        return *this;
    }
    
    MessageBuilder& payload(std::vector<std::byte> data) {
        payload_ = std::move(data);
        return *this;
    }
    
    MessageBuilder& payload(std::span<const std::byte> data) {
        payload_.assign(data.begin(), data.end());
        return *this;
    }
    
    [[nodiscard]] Message build() {
        return Message{header_, std::move(payload_)};
    }
    
private:
    MessageHeader header_;
    std::vector<std::byte> payload_;
};

} // namespace protoflow::messaging
