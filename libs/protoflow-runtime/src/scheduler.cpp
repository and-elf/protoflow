#include <protoflow/runtime/scheduler.hpp>
#include <protoflow/service/service.hpp>
#include <algorithm>
#include <stdexcept>
#include <thread>

namespace protoflow::runtime {

std::size_t Scheduler::register_service(std::shared_ptr<service::Service> service,
                                         Deadline::Duration poll_interval) {
    if (!service) {
        throw std::invalid_argument("Cannot register null service");
    }
    
    services_.emplace_back(std::move(service), poll_interval);
    return services_.size() - 1;
}

void Scheduler::unregister_service(std::size_t service_id) {
    if (service_id >= services_.size()) {
        throw std::out_of_range("Invalid service ID");
    }
    
    // Mark as disabled and clear the service pointer
    services_[service_id].enabled = false;
    services_[service_id].service.reset();
}

void Scheduler::enable_service(std::size_t service_id, bool enabled) {
    if (service_id >= services_.size()) {
        throw std::out_of_range("Invalid service ID");
    }
    
    services_[service_id].enabled = enabled;
}

std::size_t Scheduler::execute_cycle() {
    std::size_t polled_count = 0;
    
    // Poll all services whose deadline has expired
    for (auto& entry : services_) {
        if (!entry.enabled || !entry.service) {
            continue;
        }
        
        if (entry.next_poll.expired()) {
            // Poll the service (processes one message)
            entry.service->poll();
            
            // Update next poll deadline
            entry.next_poll = Deadline::from_now(entry.poll_interval);
            
            ++polled_count;
        }
    }
    
    return polled_count;
}

std::size_t Scheduler::execute_until(Deadline deadline) {
    std::size_t total_polled = 0;
    
    while (!deadline.expired()) {
        std::size_t polled = execute_cycle();
        total_polled += polled;
        
        // If nothing was polled, check if we can sleep until next deadline
        if (polled == 0) {
            auto next_ready_id = find_next_ready();
            if (!next_ready_id) {
                break; // No services to poll
            }
            
            auto& next_entry = services_[*next_ready_id];
            auto sleep_duration = std::min(
                next_entry.next_poll.remaining(),
                deadline.remaining()
            );
            
            if (sleep_duration > Deadline::Duration::zero()) {
                std::this_thread::sleep_for(sleep_duration);
            }
        }
    }
    
    return total_polled;
}

std::optional<Deadline> Scheduler::next_deadline(std::size_t service_id) const {
    if (service_id >= services_.size()) {
        return std::nullopt;
    }
    
    const auto& entry = services_[service_id];
    if (!entry.enabled || !entry.service) {
        return std::nullopt;
    }
    
    return entry.next_poll;
}

void Scheduler::clear() {
    services_.clear();
}

std::optional<std::size_t> Scheduler::find_next_ready() const {
    std::optional<std::size_t> next_id;
    Deadline earliest = Deadline::never();
    
    for (std::size_t i = 0; i < services_.size(); ++i) {
        const auto& entry = services_[i];
        if (!entry.enabled || !entry.service) {
            continue;
        }
        
        if (entry.next_poll < earliest) {
            earliest = entry.next_poll;
            next_id = i;
        }
    }
    
    return next_id;
}

} // namespace protoflow::runtime
