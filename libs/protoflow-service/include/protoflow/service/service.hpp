#pragma once

#include <optional>
#include <vector>
#include <sstream>
#include "mailbox.hpp"
#include <protoflow/messaging/message.hpp>
#include <protoflow/logging/log_message.hpp>

namespace protoflow::runtime {
class Runtime;
}

namespace protoflow::service {

// Use messaging::Message
using Message = protoflow::messaging::Message;

/// Base class for all protoflow services
/// Services read from inbound queue and write to outbound queue
/// Runtime scheduler calls poll() once per cycle per service
class Service {
public:
    virtual ~Service() = default;

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
    void on_message(Message&& msg) {
        inbound.push(std::move(msg));
    }
    
    /// Pop outbound message for routing (used by runtime)
    [[nodiscard]] std::optional<Message> pop_outbound() {
        return outbound.pop();
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

private:
    Mailbox<Message> inbound;
    Mailbox<Message> outbound;
    protoflow::messaging::ServiceId service_id_{0};

    friend class protoflow::runtime::Runtime; // Runtime needs access to outbound queue
};

} // namespace protoflow::service
