#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include "log_message.hpp"

namespace protoflow::messaging {

/// Message ID type for routing
using MessageId = uint64_t;

/// Service ID type for addressing
using ServiceId = uint32_t;

/// Message type tag for identifying payload contents
using MessageType = uint32_t;

/// Well-known message types
namespace MessageTypes {
    constexpr MessageType Unknown = 0;
    constexpr MessageType Payload = 1;
    constexpr MessageType Event = 2;
    constexpr MessageType Log = 3;
    // Libraries can define their own types starting from 100
    constexpr MessageType RpcBase = 100;
}

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
    MessageType type{MessageTypes::Unknown};
    Priority priority{Priority::Normal};
    uint64_t timestamp{0};
    
    bool operator==(const MessageHeader&) const = default;
};

/// Base message type with raw bytes and type tag
struct Message {
    MessageHeader header;
    std::vector<std::byte> data;
    
    Message() = default;
    
    Message(MessageHeader hdr, std::vector<std::byte> d)
        : header(hdr), data(std::move(d)) {}
    
    Message(MessageHeader hdr, std::span<const std::byte> d)
        : header(hdr), data(d.begin(), d.end()) {}
    
    // Copyable
    Message(const Message&) = default;
    Message& operator=(const Message&) = default;
    Message(Message&&) = default;
    Message& operator=(Message&&) = default;
    
    // Convenience accessors
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return data;
    }
    
    [[nodiscard]] std::size_t size() const noexcept {
        return data.size();
    }
    
    [[nodiscard]] MessageType type() const noexcept {
        return header.type;
    }
    
    // Type checking
    [[nodiscard]] bool is_type(const MessageType t) const noexcept {
        return header.type == t;
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
    
    MessageBuilder& type(MessageType t) {
        header_.type = t;
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
    
    MessageBuilder& payload(std::vector<std::byte> d) {
        data_ = std::move(d);
        return *this;
    }
    
    MessageBuilder& payload(std::span<const std::byte> d) {
        data_.assign(d.begin(), d.end());
        return *this;
    }
    
    [[nodiscard]] Message build() {
        return Message{header_, std::move(data_)};
    }
    
private:
    MessageHeader header_;
    std::vector<std::byte> data_;
};

} // namespace protoflow::messaging
