#include "services/app_registration_service.hpp"
#include <protoflow/logging/macros.hpp>
#include <nlohmann/json.hpp>
#include <sstream>

namespace protoflow::mainapp {

using json = nlohmann::json;

AppRegistrationService::AppRegistrationService(std::chrono::milliseconds check_interval)
    : check_interval_(check_interval)
    , last_check_(std::chrono::steady_clock::now())
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
    check_keepalives();

    // Call base class poll for message handling
    service::Service::poll();
}

bool AppRegistrationService::register_app(AppRegistration&& registration) {
    auto it = registered_apps_.find(registration.name);
    
    if (it != registered_apps_.end()) {
        // Re-registration of existing app
        PROTOFLOW_LOG_INFO(*this, "Re-registering app: " << registration.name);
        it->second.version = registration.version;
        it->second.endpoints = registration.endpoints;
        it->second.hw_requirements = registration.hw_requirements;
        it->second.last_keepalive = std::chrono::steady_clock::now();
        
        // Process FSM event
        it->second.fsm->process(AppEvent::register_app);
    } else {
        // New registration
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
        
        // Setup FSM with sink
        AppStateSink sink{registration.name};
        registration.fsm = std::make_unique<AppStateMachine<AppStateSink>>(registration.name, std::move(sink));
        
        // Process initial registration event
        registration.fsm->process(AppEvent::register_app);
        
        // Set timestamp
        registration.last_keepalive = std::chrono::steady_clock::now();
        
        // Move into map
        registered_apps_[registration.name] = std::move(registration);
    }
    
    return true;
}

void AppRegistrationService::unregister_app(const std::string& name) {
    auto it = registered_apps_.find(name);
    if (it != registered_apps_.end()) {
        // Process disconnect event before removal
        it->second.fsm->process(AppEvent::disconnect);
    }
    
    PROTOFLOW_LOG_INFO(*this, "Unregistering app: " << name);
    registered_apps_.erase(name);
}

void AppRegistrationService::update_keepalive(const std::string& name) {
    auto it = registered_apps_.find(name);
    if (it != registered_apps_.end()) {
        it->second.last_keepalive = std::chrono::steady_clock::now();
        
        // Process heartbeat event
        it->second.fsm->process(AppEvent::heartbeat);
    }
}

std::vector<const AppRegistration*> AppRegistrationService::get_registered_apps() const {
    std::vector<const AppRegistration*> apps;
    apps.reserve(registered_apps_.size());
    
    for (const auto& [name, reg] : registered_apps_) {
        apps.push_back(&reg);
    }
    
    return apps;
}

std::optional<std::reference_wrapper<const AppRegistration>> AppRegistrationService::get_app(const std::string& name) const {
    auto it = registered_apps_.find(name);
    if (it != registered_apps_.end()) {
        return std::cref(it->second);
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
        
        // Map FSM state to status string
        switch (reg.fsm->state()) {
            case AppState::unregistered:
                app_json["status"] = "unregistered";
                break;
            case AppState::registered:
                app_json["status"] = "registered";
                break;
            case AppState::alive:
                app_json["status"] = "alive";
                break;
            case AppState::dead:
                app_json["status"] = "dead";
                break;
        }
        
        app_json["endpoints"] = reg.endpoints;
        
        // In a full implementation, we would RPC to each app to get their state
        // For now, just include registration metadata
        app_json["state"] = json::object();
        
        result["apps"].push_back(std::move(app_json));
    }
    
    return result.dump(2);
}

void AppRegistrationService::handle(messaging::Message&& msg) {
    // TODO: Implement message handling based on message type
    // Will handle:
    // - Registration requests
    // - Keepalive messages
    // - State query requests
    (void)msg; // Suppress unused warning for now
}

std::vector<messaging::Message> AppRegistrationService::generate_outbound() {
    // TODO: Generate outbound messages:
    // - Registration acknowledgments
    // - State aggregation results
    return {};
}

void AppRegistrationService::check_keepalives() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto& [name, reg] : registered_apps_) {
        // Only check apps that are alive
        if (reg.fsm->state() != AppState::alive) {
            continue;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - reg.last_keepalive
        );
        
        if (elapsed > keepalive_timeout_) {
            PROTOFLOW_LOG_WARN(*this, "App keepalive timeout: " 
                               << name << " (last seen " << elapsed.count() << "s ago)");
            
            // Process timeout event
            reg.fsm->process(AppEvent::timeout);
        }
    }
}

} // namespace protoflow::mainapp
