#pragma once

#include <optional>
#include "mailbox.hpp"

namespace protoflow::service {

// Forward declarations
class Message;

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
            handle(*msg);

        // process at most one outbound message
        if (auto out = generate_outbound(); !out.empty())
            write(std::move(out.front()));       
    }

    /// Called by runtime when routing message to this service
    void on_message(Message&& msg) {
        inbound.push(std::move(msg));
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

    friend class Runtime; // Runtime needs access to outbound queue
};

} // namespace protoflow::service
