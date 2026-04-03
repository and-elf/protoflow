#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <type_traits>
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

/// Wrapper type used for header fields that accept message-type-like values.
/// Accepts enums (including `enum class`) and integral types implicitly.
struct MessageTag {
    MessageType value{0};

    constexpr MessageTag() noexcept = default;
    constexpr MessageTag(MessageType v) noexcept : value(v) {}

    template<typename E, std::enable_if_t<std::is_enum_v<E> || std::is_integral_v<E>, int> = 0>
    constexpr MessageTag(E e) noexcept : value(static_cast<MessageType>(e)) {}

    template<typename E, std::enable_if_t<std::is_enum_v<E>, int> = 0>
    constexpr operator E() const noexcept { return static_cast<E>(value); }

    constexpr operator MessageType() const noexcept { return value; }

    constexpr bool operator==(const MessageTag& o) const noexcept { return value == o.value; }
    constexpr bool operator!=(const MessageTag& o) const noexcept { return !(*this == o); }
};

// Comparison helpers to avoid ambiguous conversions with built-in operators.
constexpr bool operator==(const MessageTag& a, MessageType b) noexcept { return a.value == b; }
constexpr bool operator==(MessageType a, const MessageTag& b) noexcept { return a == b.value; }
constexpr bool operator!=(const MessageTag& a, MessageType b) noexcept { return !(a == b); }
constexpr bool operator!=(MessageType a, const MessageTag& b) noexcept { return !(a == b); }

/// Well-known message types
namespace MessageTypes {
    constexpr MessageType Unknown = 0;
    constexpr MessageType Payload = 1;
    constexpr MessageType Event = 2;
    constexpr MessageType Log = 3;
    // Libraries can define their own types starting from 100
    constexpr MessageType RpcBase = 100;
    constexpr MessageType AppRegistrationRequests = 120;
    constexpr MessageType AppRegistrationResponses = 130;
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
    MessageTag type{MessageTypes::Unknown};
    Priority priority{Priority::Normal};
    int64_t timestamp{0};
    
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
    
    [[nodiscard]] MessageTag type() const noexcept {
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
    
    MessageBuilder& type(MessageTag t) {
        header_.type = t;
        return *this;
    }
    
    MessageBuilder& priority(Priority p) {
        header_.priority = p;
        return *this;
    }
    
    MessageBuilder& timestamp(int64_t ts) {
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
