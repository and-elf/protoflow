#pragma once

#include <chrono>
#include <cstdint>

namespace protoflow::runtime {

/// Deadline for scheduling and timing control
/// Used for deterministic execution and timeout management
class Deadline {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;
    
    /// Create a deadline from now + duration
    static Deadline from_now(Duration duration) {
        return Deadline{Clock::now() + duration};
    }
    
    /// Create a deadline at a specific time point
    static Deadline at(TimePoint time) {
        return Deadline{time};
    }
    
    /// Create a deadline that has already passed
    static Deadline immediate() {
        return Deadline{TimePoint::min()};
    }
    
    /// Create a deadline that never expires
    static Deadline never() {
        return Deadline{TimePoint::max()};
    }
    
    /// Check if deadline has expired
    [[nodiscard]] bool expired() const noexcept {
        return Clock::now() >= deadline_;
    }
    
    /// Get time remaining until deadline
    [[nodiscard]] Duration remaining() const noexcept {
        auto now = Clock::now();
        if (now >= deadline_) {
            return Duration::zero();
        }
        return deadline_ - now;
    }
    
    /// Get the absolute time point of the deadline
    [[nodiscard]] TimePoint time_point() const noexcept {
        return deadline_;
    }
    
    /// Comparison operators
    bool operator==(const Deadline& other) const = default;
    auto operator<=>(const Deadline& other) const = default;
    
private:
    explicit Deadline(TimePoint time) : deadline_(time) {}
    
    TimePoint deadline_;
};

} // namespace protoflow::runtime
