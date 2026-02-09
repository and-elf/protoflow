#pragma once

#include "scheduler.hpp"
#include "deadline.hpp"
#include <protoflow/messaging/router.hpp>
#include <protoflow/messaging/message.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

namespace protoflow::service {
class Service;
}

namespace protoflow::runtime {

/// Core runtime system
/// Manages message routing and service scheduling
/// Provides deterministic execution guarantees
class Runtime {
public:
    /// Configuration for runtime behavior
    struct Config {
        /// Default polling interval for services
        Deadline::Duration default_poll_interval = std::chrono::milliseconds(10);
        
        /// Maximum messages to route per cycle
        std::size_t max_messages_per_cycle = 100;
        
        /// Enable strict determinism (one message per service per cycle)
        bool strict_determinism = true;
    };
    
    Runtime();
    explicit Runtime(Config config);
    ~Runtime();
    
    // Non-copyable, movable
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) noexcept = default;
    Runtime& operator=(Runtime&&) noexcept = default;
    
    /// Register a service with the runtime
    /// Returns service ID for routing and reference
    messaging::ServiceId register_service(std::shared_ptr<service::Service> service,
                                          Deadline::Duration poll_interval = std::chrono::milliseconds(10));
    
    /// Unregister a service by ID
    void unregister_service(messaging::ServiceId service_id);
    
    /// Add a routing rule for messages
    void add_route(messaging::MessageId msg_id, messaging::ServiceId destination);
    
    /// Add a conditional routing rule
    void add_route(messaging::RoutePredicate predicate, 
                   std::vector<messaging::ServiceId> destinations);
    
    /// Start the runtime and all registered services
    void start();
    
    /// Stop the runtime and all registered services
    void stop();
    
    /// Execute one runtime cycle:
    /// 1. Route pending messages
    /// 2. Execute scheduler cycle (poll ready services)
    /// Returns number of messages routed and services polled
    struct CycleStats {
        std::size_t messages_routed{0};
        std::size_t services_polled{0};
    };
    CycleStats execute_cycle();
    
    /// Execute cycles until deadline
    /// Returns aggregate statistics
    CycleStats execute_until(Deadline deadline);
    
    /// Execute for a specific duration
    CycleStats execute_for(Deadline::Duration duration);
    
    /// Send a message into the runtime
    /// Message will be routed on next cycle
    void send_message(messaging::Message&& msg);
    
    /// Get runtime statistics
    struct Stats {
        std::size_t total_messages_routed{0};
        std::size_t total_cycles{0};
        std::size_t registered_services{0};
    };
    [[nodiscard]] Stats stats() const noexcept;
    
    /// Check if runtime is running
    [[nodiscard]] bool is_running() const noexcept {
        return running_;
    }
    
private:
    Config config_;
    Scheduler scheduler_;
    messaging::Router router_;
    
    /// Service registry: service_id -> service
    std::unordered_map<messaging::ServiceId, std::shared_ptr<service::Service>> services_;
    
    /// Next service ID to assign
    messaging::ServiceId next_service_id_{1};
    
    /// Pending messages to route
    std::vector<messaging::Message> pending_messages_;
    
    /// Runtime state
    bool running_{false};
    
    /// Statistics
    Stats stats_;
    
    /// Route all pending messages to their destinations
    std::size_t route_pending_messages();
    
    /// Deliver a message to a service
    void deliver_message(messaging::ServiceId service_id, messaging::Message&& msg);
    
    /// Collect outbound messages from services
    void collect_outbound_messages();
};

} // namespace protoflow::runtime
