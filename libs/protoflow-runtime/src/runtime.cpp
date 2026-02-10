#include <protoflow/runtime/runtime.hpp>
#include <protoflow/service/service.hpp>
#include <algorithm>
#include <stdexcept>
#include <thread>

namespace protoflow::runtime {

Runtime::Runtime() 
    : Runtime(Config{})
{}

Runtime::Runtime(Config config) 
    : config_(std::move(config))
    , scheduler_()
    , router_()
{}

Runtime::~Runtime() {
    if (running_) {
        stop();
    }
}

messaging::ServiceId Runtime::register_service(std::shared_ptr<service::Service> service,
                                                Deadline::Duration poll_interval) {
    if (!service) {
        throw std::invalid_argument("Cannot register null service");
    }
    
    auto service_id = next_service_id_++;
    
    // Register with scheduler
    scheduler_.register_service(service, poll_interval);
    
    // Store in service registry
    services_[service_id] = std::move(service);
    
    ++stats_.registered_services;
    
    return service_id;
}

void Runtime::unregister_service(messaging::ServiceId service_id) {
    auto it = services_.find(service_id);
    if (it == services_.end()) {
        throw std::out_of_range("Invalid service ID");
    }
    
    // Note: We can't easily unregister from scheduler by service_id
    // since scheduler uses indices. For now, we just remove from our registry.
    // In production, we'd want a better mapping.
    services_.erase(it);
    
    if (stats_.registered_services > 0) {
        --stats_.registered_services;
    }
}

void Runtime::add_route(messaging::MessageId msg_id, messaging::ServiceId destination) {
    router_.add_route_by_id(msg_id, destination);
}

void Runtime::add_route(messaging::RoutePredicate predicate, 
                        std::vector<messaging::ServiceId> destinations) {
    router_.add_route(std::move(predicate), std::move(destinations));
}

void Runtime::start() {
    if (running_) {
        return;
    }
    
    // Start all registered services
    for (auto& [id, service] : services_) {
        if (service) {
            service->start();
        }
    }
    
    running_ = true;
}

void Runtime::stop() {
    if (!running_) {
        return;
    }
    
    // Stop all registered services
    for (auto& [id, service] : services_) {
        if (service) {
            service->stop();
        }
    }
    
    running_ = false;
}

Runtime::CycleStats Runtime::execute_cycle() {
    CycleStats cycle_stats;
    
    // Phase 1: Route pending messages
    cycle_stats.messages_routed = route_pending_messages();
    
    // Phase 2: Execute scheduler cycle (poll services)
    cycle_stats.services_polled = scheduler_.execute_cycle();
    
    // Phase 3: Collect outbound messages from services
    collect_outbound_messages();
    
    // Update statistics
    stats_.total_messages_routed += cycle_stats.messages_routed;
    ++stats_.total_cycles;
    
    return cycle_stats;
}

Runtime::CycleStats Runtime::execute_until(Deadline deadline) {
    CycleStats total_stats;
    
    while (!deadline.expired()) {
        auto cycle_stats = execute_cycle();
        total_stats.messages_routed += cycle_stats.messages_routed;
        total_stats.services_polled += cycle_stats.services_polled;
        
        // If nothing happened, sleep briefly to avoid busy waiting
        if (cycle_stats.messages_routed == 0 && cycle_stats.services_polled == 0) {
            auto sleep_duration = std::min(
                config_.default_poll_interval,
                deadline.remaining()
            );
            if (sleep_duration > Deadline::Duration::zero()) {
                std::this_thread::sleep_for(sleep_duration);
            }
        }
    }
    
    return total_stats;
}

Runtime::CycleStats Runtime::execute_for(Deadline::Duration duration) {
    return execute_until(Deadline::from_now(duration));
}

void Runtime::send_message(messaging::Message&& msg) {
    pending_messages_.push_back(std::move(msg));
}

Runtime::Stats Runtime::stats() const noexcept {
    return stats_;
}

std::size_t Runtime::route_pending_messages() {
    std::size_t routed_count = 0;
    std::size_t max_messages = config_.max_messages_per_cycle;
    
    // Process up to max_messages_per_cycle
    while (!pending_messages_.empty() && routed_count < max_messages) {
        auto msg = std::move(pending_messages_.front());
        pending_messages_.erase(pending_messages_.begin());
        
        // Route message to destination(s)
        auto destinations = router_.route(msg);
        
        for (auto dest_id : destinations) {
            // Create a copy for each destination (except the last one)
            if (dest_id != destinations.back()) {
                messaging::Message msg_copy;
                msg_copy.header = msg.header;
                msg_copy.data = msg.data; // Copy data
                deliver_message(dest_id, std::move(msg_copy));
            } else {
                // Move the original message to the last destination
                deliver_message(dest_id, std::move(msg));
            }
        }
        
        ++routed_count;
    }
    
    return routed_count;
}

void Runtime::deliver_message(messaging::ServiceId service_id, messaging::Message&& msg) {
    auto it = services_.find(service_id);
    if (it == services_.end()) {
        // Service not found - could log this as a routing error
        return;
    }
    
    // Deliver message to service's inbound queue
    // Note: We need to access the service's on_message method
    // This requires friendship or a public interface
    // For now, we'll assume Service exposes this or Runtime is a friend
    if (it->second) {
        it->second->on_message(std::move(msg));
    }
}

void Runtime::collect_outbound_messages() {
    // Collect outbound messages from all services
    for (auto& [id, service] : services_) {
        if (!service) {
            continue;
        }
        
        // Pop all outbound messages and add to pending queue
        while (auto msg = service->pop_outbound()) {
            pending_messages_.push_back(std::move(*msg));
        }
    }
}

} // namespace protoflow::runtime
