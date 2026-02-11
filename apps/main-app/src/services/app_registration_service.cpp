#include "services/app_registration_service.hpp"
#include "messages.hpp"
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
    // Call base class poll for message handling
    service::Service::poll();
    // Check for stale keepalives periodically
    check_keepalives();

}

bool AppRegistrationService::register_app(AppRegistration&& registration) {
    if (auto app = get_app(registration.name)) {
        // Re-registration of existing app
        auto& reg = app->get();
        PROTOFLOW_LOG_INFO(*this, "Re-registering app: " << registration.name);
        reg.version = registration.version;
        reg.endpoints = registration.endpoints;
        reg.hw_requirements = registration.hw_requirements;
        reg.last_keepalive = std::chrono::steady_clock::now();
        
        // Process FSM event
        reg.fsm->process(AppEvent::register_app);
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
    if (auto app = get_app(name)) {
        // Process disconnect event before removal
        app->get().fsm->process(AppEvent::disconnect);
        PROTOFLOW_LOG_INFO(*this, "Unregistering app: " << name);
        registered_apps_.erase(name);
    }
}

void AppRegistrationService::update_keepalive(const std::string& name) {
    if (auto app = get_app(name)) {
        auto& reg = app->get();
        reg.last_keepalive = std::chrono::steady_clock::now();
        
        // Process heartbeat event
        reg.fsm->process(AppEvent::heartbeat);
    }
}

[[nodiscard]] std::vector<const AppRegistration*> AppRegistrationService::get_registered_apps() const {
    std::vector<const AppRegistration*> apps;
    apps.reserve(registered_apps_.size());
    
    for (const auto& [name, reg] : registered_apps_) {
        apps.push_back(&reg);
    }
    
    return apps;
}

[[nodiscard]] std::optional<std::reference_wrapper<const AppRegistration>> AppRegistrationService::get_app(const std::string& name) const {
    if (auto it = registered_apps_.find(name); it != registered_apps_.end()) {
        return std::cref(it->second);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::reference_wrapper<AppRegistration>> AppRegistrationService::get_app(const std::string& name) {
    if (auto it = registered_apps_.find(name); it != registered_apps_.end()) {
        return std::ref(it->second);
    }
    return std::nullopt;
}

[[nodiscard]] std::string AppRegistrationService::aggregate_state_json() const {
    json result{
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
        {"apps", json::array()}
    };
    
    for (const auto& [name, reg] : registered_apps_) {
        const auto state_to_string = [](AppState state) -> std::string_view {
            switch (state) {
                case AppState::unregistered: return "unregistered";
                case AppState::registered: return "registered";
                case AppState::alive: return "alive";
                case AppState::dead: return "dead";
            }
            return "unknown";
        };
        
        json app_json{
            {"name", name},
            {"version", reg.version},
            {"status", state_to_string(reg.fsm->state())},
            {"endpoints", reg.endpoints},
            {"state", json::object()}
        };
        
        // In a full implementation, we would RPC to each app to get their state
        // For now, just include registration metadata
        
        result["apps"].push_back(std::move(app_json));
    }
    
    return result.dump(2);
}

void AppRegistrationService::handle(messaging::Message&& msg) {
    switch (msg.type()) {
        case MessageTypes::AppRegistrationEvent: {
            if (auto event = AppRegistrationEvent::deserialize(msg.bytes())) {
                AppRegistration registration;
                registration.name = std::move(event->app_name);
                registration.version = std::move(event->version);
                registration.endpoints = std::move(event->endpoints);
                register_app(std::move(registration));
            }
            break;
        }
        
        case MessageTypes::AppUnregistrationEvent: {
            if (auto event = AppUnregistrationEvent::deserialize(msg.bytes())) {
                unregister_app(event->app_name);
            }
            break;
        }
        
        default:
            // Unknown message type, ignore
            break;
    }
}

std::vector<messaging::Message> AppRegistrationService::generate_outbound() {
    // No outbound messages generated - using direct method calls instead
    return {};
}

void AppRegistrationService::check_keepalives() {
    const auto now = std::chrono::steady_clock::now();
    
    for (auto& [name, reg] : registered_apps_) {
        // Only check apps that are alive
        if (reg.fsm->state() != AppState::alive) {
            continue;
        }
        
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
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
