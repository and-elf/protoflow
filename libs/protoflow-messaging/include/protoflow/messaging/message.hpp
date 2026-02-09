#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <span>
#include "log_message.hpp"

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

/// Raw payload message (binary data)
struct PayloadMessage {
    std::vector<std::byte> data;
    
    PayloadMessage() = default;
    explicit PayloadMessage(std::vector<std::byte> d) : data(std::move(d)) {}
    
    // Make copyable for message routing to multiple destinations
    PayloadMessage(const PayloadMessage&) = default;
    PayloadMessage& operator=(const PayloadMessage&) = default;
    PayloadMessage(PayloadMessage&&) = default;
    PayloadMessage& operator=(PayloadMessage&&) = default;
};

/// Event message (string-based event notification)
struct EventMessage {
    std::string event_type;
    std::string event_data;
    
    EventMessage() = default;
    EventMessage(std::string type, std::string data = "")
        : event_type(std::move(type)), event_data(std::move(data)) {}
};

/// Message payload variant type
/// Can hold LogMessage, PayloadMessage (raw bytes), or EventMessage
using MessagePayload = std::variant<
    std::monostate,  // Empty/uninitialized
    PayloadMessage,
    EventMessage,
    LogMessage
>;

/// Base message type
/// All messages must be movable and have a header
struct Message {
    MessageHeader header;
    MessagePayload payload;
    
    Message() = default;
    
    Message(MessageHeader hdr, MessagePayload p)
        : header(hdr), payload(std::move(p)) {}
    
    // Legacy constructor for backward compatibility with raw bytes
    Message(MessageHeader hdr, std::vector<std::byte> data)
        : header(hdr), payload(PayloadMessage{std::move(data)}) {}
    
    // Make copyable to support message routing to multiple destinations
    Message(const Message&) = default;
    Message& operator=(const Message&) = default;
    Message(Message&&) = default;
    Message& operator=(Message&&) = default;
    
    // Legacy data access for backward compatibility
    [[nodiscard]] std::span<const std::byte> data() const noexcept {
        if (auto* p = std::get_if<PayloadMessage>(&payload)) {
            return p->data;
        }
        static const std::vector<std::byte> empty;
        return empty;
    }
    
    [[nodiscard]] std::size_t size() const noexcept {
        if (auto* p = std::get_if<PayloadMessage>(&payload)) {
            return p->data.size();
        }
        return 0;
    }
    
    // Helpers to check payload type
    [[nodiscard]] bool is_log() const noexcept {
        return std::holds_alternative<LogMessage>(payload);
    }
    
    [[nodiscard]] bool is_payload() const noexcept {
        return std::holds_alternative<PayloadMessage>(payload);
    }
    
    [[nodiscard]] bool is_event() const noexcept {
        return std::holds_alternative<EventMessage>(payload);
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
        payload_ = PayloadMessage{std::move(data)};
        return *this;
    }
    
    MessageBuilder& payload(std::span<const std::byte> data) {
        std::vector<std::byte> vec;
        vec.assign(data.begin(), data.end());
        payload_ = PayloadMessage{std::move(vec)};
        return *this;
    }
    
    MessageBuilder& payload(MessagePayload p) {
        payload_ = std::move(p);
        return *this;
    }
    
    MessageBuilder& event(std::string type, std::string data = "") {
        payload_ = EventMessage{std::move(type), std::move(data)};
        return *this;
    }
    
    MessageBuilder& log(LogMessage log_msg) {
        payload_ = std::move(log_msg);
        return *this;
    }
    
    [[nodiscard]] Message build() {
        return Message{header_, std::move(payload_)};
    }
    
private:
    MessageHeader header_;
    MessagePayload payload_;
};

} // namespace protoflow::messaging
