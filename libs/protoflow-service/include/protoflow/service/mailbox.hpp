#pragma once

#include <optional>
#include <queue>
#include <mutex>

namespace protoflow::service {

/// Thread-safe message queue for service communication
/// Services have inbound and outbound mailboxes
template<typename T>
class Mailbox {
public:
    /// Push message to queue (thread-safe)
    void push(T&& item) {
        std::lock_guard lock(mutex_);
        queue_.push(std::move(item));
    }

    /// Pop message from queue (non-blocking, thread-safe)
    [[nodiscard]] std::optional<T> pop() {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /// Check if mailbox is empty
    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    /// Get number of messages in mailbox
    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
};

} // namespace protoflow::service
