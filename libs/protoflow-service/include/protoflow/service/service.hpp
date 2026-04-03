#pragma once

#include <optional>
#include <vector>
#include <sstream>
#include <array>
#include <type_traits>
#include <initializer_list>
#include <utility>
#include "mailbox.hpp"
#include <protoflow/messaging/message.hpp>
#include <protoflow/logging/log_message.hpp>

namespace protoflow::runtime {
class Runtime;
}

namespace protoflow::service {

// Use messaging::Message
using Message = protoflow::messaging::Message;

// Trait-based enumeration point for enums. Specialize `enum_traits<MyEnum>`
// for your enum to provide a compile-time list of enumerators:
//
// template<> struct enum_traits<MyEnum> {
//   static constexpr std::array<MyEnum,3> values{ MyEnum::A, MyEnum::B, MyEnum::C };
// };
template<typename E>
struct enum_traits {
    static constexpr std::array<E, 0> values{};
};

template<typename> inline constexpr bool always_false_v = false;

// Try to discover a sentinel/count for an enum type using common names.
template<typename E>
constexpr std::optional<std::size_t> discover_enum_count() {
    if constexpr (!std::is_enum_v<E>) {
        return std::nullopt;
    } else if constexpr (requires { E::COUNT; }) {
        return static_cast<std::size_t>(E::COUNT);
    } else if constexpr (requires { E::COUNT_; }) {
        return static_cast<std::size_t>(E::COUNT_);
    } else if constexpr (requires { E::LAST; }) {
        using U = std::underlying_type_t<E>;
        return static_cast<std::size_t>(static_cast<U>(E::LAST)) + 1;
    } else if constexpr (requires { E::Last; }) {
        using U = std::underlying_type_t<E>;
        return static_cast<std::size_t>(static_cast<U>(E::Last)) + 1;
    } else if constexpr (requires { E::kLast; }) {
        using U = std::underlying_type_t<E>;
        return static_cast<std::size_t>(static_cast<U>(E::kLast)) + 1;
    } else {
        return std::nullopt;
    }
}

/// Base class for all protoflow services
/// Services read from inbound queue and write to outbound queue
/// Runtime scheduler calls poll() once per cycle per service
class Service {
public:
    virtual ~Service() = default;

    /// Default construct a service. Subscriptions are internal and set by
    /// derived classes in their constructors (use the protected ctor).
    Service() = default;

    /// Called by runtime when service is started
    /// Use for initialization, resource acquisition, etc.
    virtual void start() {}

    /// Called by runtime when service is stopped
    /// Use for cleanup, resource release, etc.
    virtual void stop() {}

    /// Called by runtime scheduler
    /// One message per service per cycle (deterministic execution)
    virtual void poll() {
        // Read one message from inbound queue
        // process at most one inbound message
        if (auto msg = read(); msg)
            handle(std::move(*msg));

        // process at most one outbound message
        if (auto out = generate_outbound(); !out.empty())
            write(std::move(out.front()));       
    }

    /// Called by runtime when routing message to this service
    /// The base implementation filters messages according to `get_message_types()`.
    void on_message(Message&& msg) {
        auto types = get_message_types();
        if (!types.empty()) {
            bool interested = false;
            for (auto t : types) {
                if (t == msg.type()) { interested = true; break; }
            }
            if (!interested) return; // drop message not subscribed to
        }
        inbound.push(std::move(msg));
    }
    
    /// Pop outbound message for routing (used by runtime)
    [[nodiscard]] std::optional<Message> pop_outbound() {
        return outbound.pop();
    }

    /// Return list of message types this service is interested in.
    /// Empty vector means "subscribe to all" (wildcard).
    virtual std::vector<protoflow::messaging::MessageType> get_message_types() const {
        return subscriptions_;
    }

protected:
    /// Read next message from inbound queue (non-blocking)
    [[nodiscard]] std::optional<Message> read() {
        return inbound.pop();
    }

    /// Queue message for outbound routing
    void write(Message&& msg) {
        outbound.push(std::move(msg));
    }

    /// Log a message at specified level
    void log(protoflow::messaging::LogLevel level, const std::string& text) {
        using namespace protoflow::messaging;
        
        auto log_msg = LogMessage{level, text};
        // TODO: Serialize log message to bytes
        auto msg = MessageBuilder{}
            .from(service_id_)
            .type(MessageTypes::Log)
            .build();
        
        write(std::move(msg));
    }
    
    /// Convenience logging methods
    void log_trace(const std::string& text) { log(protoflow::messaging::LogLevel::Trace, text); }
    void log_debug(const std::string& text) { log(protoflow::messaging::LogLevel::Debug, text); }
    void log_info(const std::string& text)  { log(protoflow::messaging::LogLevel::Info, text); }
    void log_warn(const std::string& text)  { log(protoflow::messaging::LogLevel::Warn, text); }
    void log_error(const std::string& text) { log(protoflow::messaging::LogLevel::Error, text); }
    void log_fatal(const std::string& text) { log(protoflow::messaging::LogLevel::Fatal, text); }
    
    /// Set service ID (called by runtime during registration)
    void set_service_id(protoflow::messaging::ServiceId id) {
        service_id_ = id;
    }

    /// Override to handle incoming messages
    virtual void handle(Message&& msg) = 0;

    /// Override to generate outbound messages
    /// Called after handling inbound message
    virtual std::vector<Message> generate_outbound() {
        return {};
    }

    protoflow::messaging::ServiceId service_id_{0};

protected:
    /// Accepts an initializer_list of enum/integral values convertible to MessageType.
    template<typename E>
    explicit Service(std::initializer_list<E> list)
    requires (std::is_convertible_v<E, protoflow::messaging::MessageType> || std::is_enum_v<E>)
    {
        subscriptions_.reserve(list.size());
        for (auto v : list) {
            subscriptions_.push_back(static_cast<protoflow::messaging::MessageType>(v));
        }
    }

    // (Non-templated) Service keeps subscriptions_ for derived classes to set.

    // Static list of message types this service subscribes to.
    // Initialized in the constructor of the derived class and not modified at runtime.
    std::vector<protoflow::messaging::MessageType> subscriptions_{};

private:
    Mailbox<Message> inbound;
    Mailbox<Message> outbound;

    friend class protoflow::runtime::Runtime; // Runtime needs access to outbound queue
};

} // namespace protoflow::service
