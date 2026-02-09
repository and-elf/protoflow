#pragma once

#include "message.hpp"
#include <optional>
#include <functional>
#include <unordered_map>

namespace protoflow::messaging {

/// Route destination for a message
struct Route {
    ServiceId destination;
    
    bool operator==(const Route&) const = default;
};

/// Routing rule predicate
using RoutePredicate = std::function<bool(const Message&)>;

/// Routing table entry
struct RouteEntry {
    RoutePredicate predicate;
    std::vector<ServiceId> destinations;
};

/// Message router for directing messages to services
/// Routes are evaluated in order until a match is found
class Router {
public:
    /// Add a route based on destination service ID
    void add_route(ServiceId destination) {
        default_destinations_.push_back(destination);
    }
    
    /// Add a conditional route
    void add_route(RoutePredicate predicate, std::vector<ServiceId> destinations) {
        routes_.push_back(RouteEntry{
            .predicate = std::move(predicate),
            .destinations = std::move(destinations)
        });
    }
    
    /// Add a route for a specific message ID
    void add_route_by_id(MessageId msg_id, ServiceId destination) {
        id_routes_[msg_id].push_back(destination);
    }
    
    /// Route a message to its destination(s)
    /// Returns list of service IDs to deliver to
    [[nodiscard]] std::vector<ServiceId> route(const Message& msg) const {
        // First check message header destination
        if (msg.header.destination != 0) {
            return {msg.header.destination};
        }
        
        // Check ID-based routes
        if (auto it = id_routes_.find(msg.header.id); it != id_routes_.end()) {
            return it->second;
        }
        
        // Check conditional routes
        for (const auto& entry : routes_) {
            if (entry.predicate(msg)) {
                return entry.destinations;
            }
        }
        
        // Return default destinations
        return default_destinations_;
    }
    
    /// Clear all routes
    void clear() {
        routes_.clear();
        id_routes_.clear();
        default_destinations_.clear();
    }
    
private:
    std::vector<RouteEntry> routes_;
    std::unordered_map<MessageId, std::vector<ServiceId>> id_routes_;
    std::vector<ServiceId> default_destinations_;
};

} // namespace protoflow::messaging
