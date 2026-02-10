#include "services/app_registration_service.hpp"
#include <protoflow/logging/macros.hpp>
#include <nlohmann/json.hpp>
#include <sstream>

namespace protoflow::mainapp {

using json = nlohmann::json;

AppRegistrationService::AppRegistrationService()
    : last_check_(std::chrono::steady_clock::now())
{
}

AppRegistrationService::~AppRegistrationService() = default;

void AppRegistrationService::start() {
    PROTOFLOW_LOG_INFO(*this, "Started");
}

void AppRegistrationService::stop() {
    PROTOFLOW_LOG_INFO(*this, "Stopped");
    registered_apps_.clear();
}

void AppRegistrationService::poll() {
    // Check for stale keepalives periodically
    auto now = std::chrono::steady_clock::now();
    if (now - last_check_ > std::chrono::seconds(5)) {
        check_keepalives();
        last_check_ = now;
    }

    // Call base class poll for message handling
    service::Service::poll();
}

bool AppRegistrationService::register_app(const AppRegistration& registration) {
    auto it = registered_apps_.find(registration.name);
    if (it != registered_apps_.end()) {
        PROTOFLOW_LOG_INFO(*this, "Re-registering app: " << registration.name);
    } else {
        PROTOFLOW_LOG_INFO(*this, "Registering new app: " << registration.name);
        PROTOFLOW_LOG_INFO(*this, "  - Version: " << registration.version);
        
        std::ostringstream endpoints_oss;
        for (const auto& ep : registration.endpoints) {
            endpoints_oss << ep << " ";
        }
        PROTOFLOW_LOG_INFO(*this, "  - Endpoints: " << endpoints_oss.str());
        
        if (!registration.hw_requirements.empty()) {
            std::ostringstream hw_oss;
            for (const auto& hw : registration.hw_requirements) {
                hw_oss << hw << " ";
            }
            PROTOFLOW_LOG_INFO(*this, "  - Hardware requirements: " << hw_oss.str());
        }
    }

    auto reg = registration;
    reg.last_keepalive = std::chrono::steady_clock::now();
    reg.active = true;
    registered_apps_[registration.name] = std::move(reg);
    
    return true;
}

void AppRegistrationService::unregister_app(const std::string& name) {
    PROTOFLOW_LOG_INFO(*this, "Unregistering app: " << name);
    registered_apps_.erase(name);
}

void AppRegistrationService::update_keepalive(const std::string& name) {
    auto it = registered_apps_.find(name);
    if (it != registered_apps_.end()) {
        it->second.last_keepalive = std::chrono::steady_clock::now();
        if (!it->second.active) {
            PROTOFLOW_LOG_INFO(*this, "App reconnected: " << name);
            it->second.active = true;
        }
    }
}

std::vector<AppRegistration> AppRegistrationService::get_registered_apps() const {
    std::vector<AppRegistration> apps;
    apps.reserve(registered_apps_.size());
    
    for (const auto& [name, reg] : registered_apps_) {
        apps.push_back(reg);
    }
    
    return apps;
}

std::optional<AppRegistration> AppRegistrationService::get_app(const std::string& name) const {
    auto it = registered_apps_.find(name);
    if (it != registered_apps_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string AppRegistrationService::aggregate_state_json() const {
    json result;
    result["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    result["apps"] = json::array();
    
    for (const auto& [name, reg] : registered_apps_) {
        json app_json;
        app_json["name"] = name;
        app_json["version"] = reg.version;
        app_json["status"] = reg.active ? "alive" : "stale";
        app_json["endpoints"] = reg.endpoints;
        
        // In a full implementation, we would RPC to each app to get their state
        // For now, just include registration metadata
        app_json["state"] = json::object();
        
        result["apps"].push_back(std::move(app_json));
    }
    
    return result.dump(2);
}

void AppRegistrationService::handle(messaging::Message&& msg) {
    // Handle incoming messages from other services or RPC clients
    // This would include:
    // - Registration requests
    // - Keepalive messages
    // - Fragment requests
    // - State query requests
    
    // Placeholder implementation
}

std::vector<messaging::Message> AppRegistrationService::generate_outbound() {
    // Generate outbound messages for:
    // - Registration acknowledgments
    // - Fragment responses
    // - State aggregation results
    
    return {};
}

void AppRegistrationService::check_keepalives() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto& [name, reg] : registered_apps_) {
        if (!reg.active) continue;
        
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - reg.last_keepalive
        );
        
        if (elapsed > keepalive_timeout_) {
            PROTOFLOW_LOG_WARN(*this, "App keepalive timeout: " 
                               << name << " (last seen " << elapsed.count() << "s ago)");
            reg.active = false;
        }
    }
}

} // namespace protoflow::mainapp
