#pragma once

#include "deadline.hpp"
#include <vector>
#include <memory>
#include <optional>

namespace protoflow::service {
class Service;
}

namespace protoflow::runtime {

/// Scheduler for deterministic service execution
/// Executes one message per service per cycle
class Scheduler {
public:
    /// Service entry with scheduling metadata
    struct ServiceEntry {
        std::shared_ptr<service::Service> service;
        Deadline next_poll;
        Deadline::Duration poll_interval;
        bool enabled{true};
        
        ServiceEntry(std::shared_ptr<service::Service> svc, 
                     Deadline::Duration interval)
            : service(std::move(svc))
            , next_poll(Deadline::immediate())
            , poll_interval(interval)
        {}
    };
    
    /// Register a service with the scheduler
    /// Returns service ID for later reference
    std::size_t register_service(std::shared_ptr<service::Service> service,
                                  Deadline::Duration poll_interval);
    
    /// Unregister a service by ID
    void unregister_service(std::size_t service_id);
    
    /// Enable or disable a service
    void enable_service(std::size_t service_id, bool enabled);
    
    /// Execute one scheduling cycle
    /// Returns number of services polled
    std::size_t execute_cycle();
    
    /// Execute cycles until deadline
    /// Returns total number of services polled
    std::size_t execute_until(Deadline deadline);
    
    /// Get number of registered services
    [[nodiscard]] std::size_t service_count() const noexcept {
        return services_.size();
    }
    
    /// Get next poll deadline for a service
    [[nodiscard]] std::optional<Deadline> next_deadline(std::size_t service_id) const;
    
    /// Clear all registered services
    void clear();
    
private:
    std::vector<ServiceEntry> services_;
    
    /// Find next service to poll based on deadline
    [[nodiscard]] std::optional<std::size_t> find_next_ready() const;
};

} // namespace protoflow::runtime
