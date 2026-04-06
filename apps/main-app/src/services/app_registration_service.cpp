#include "services/app_registration_service.hpp"
#include <protoflow/logging/macros.hpp>
#include <protoflow/messages.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <protoflow/messaging/message.hpp>

namespace protoflow::mainapp {

using json = nlohmann::json; 
using namespace protoflow::app_registration_protocol;
AppRegistrationService::AppRegistrationService(std::chrono::milliseconds check_interval)
    : Service({request::hello,
             request::register_app,
             request::heartbeat,
             request::unregister_app})
    , check_interval_(check_interval)
    , last_check_(std::chrono::steady_clock::now()) {}
AppRegistrationService::~AppRegistrationService() = default; 

void AppRegistrationService::start() { PROTOFLOW_LOG_INFO(*this, "Started"); }

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

bool AppRegistrationService::register_app(std::optional<AppRegistrationEvent> event) {
    if (!event) return false;
    
    AppRegistration registration;
    registration.name = std::move(event->app_name);
    registration.endpoints = std::move(event->endpoints);
    
    return register_app(std::move(registration));
}

bool AppRegistrationService::register_app(AppRegistration&& registration) {
    if (auto app = get_app(registration.name)) {
        // Re-registration of existing app
        auto& reg = app->get();
        PROTOFLOW_LOG_INFO(*this, "Re-registering app: " << registration.name);
        reg.endpoints = registration.endpoints;
        reg.hw_requirements = registration.hw_requirements;
        reg.last_keepalive = std::chrono::steady_clock::now();
        
        // Process FSM event
        reg.fsm->process(AppEvent::register_app);
    } else {
        // New registration
        PROTOFLOW_LOG_INFO(*this, "Registering new app: " << registration.name);
        
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
        
        // Save name and endpoints BEFORE moving registration
        std::string app_name = registration.name;
        std::vector<std::string> endpoints = registration.endpoints;
        
        // Move into map
        registered_apps_[registration.name] = std::move(registration);
        
        // Publish registration event to message bus for HTTPService and other subscribers
        AppRegistrationEvent reg_event;
        reg_event.app_name = std::move(app_name);
        reg_event.endpoints = std::move(endpoints);
        
        auto payload = reg_event.serialize();
        auto msg = messaging::MessageBuilder{}
            .from(service_id_)
            .type(AppMessageTypes::AppRegistrationEvent)
            .payload(std::move(payload))
            .build();
        pending_outbound_.push_back(std::move(msg));
        
        return true;
    }
    
    // For re-registration case, also publish the event
    AppRegistrationEvent reg_event;
    reg_event.app_name = registration.name;
    reg_event.endpoints = registration.endpoints;
    
    auto payload = reg_event.serialize();
    auto msg = messaging::MessageBuilder{}
        .from(service_id_)
        .type(AppMessageTypes::AppRegistrationEvent)
        .payload(std::move(payload))
        .build();
    pending_outbound_.push_back(std::move(msg));
    
    return true;
}

void AppRegistrationService::unregister_app(const std::string& name) {
    if (auto app = get_app(name)) {
        // Process disconnect event before removal
        app->get().fsm->process(AppEvent::disconnect);
        PROTOFLOW_LOG_INFO(*this, "Unregistering app: " << name);
        registered_apps_.erase(name);
        
        // Publish unregistration event to message bus
        AppUnregistrationEvent unreg_event;
        unreg_event.app_name = name;
        
        auto payload = unreg_event.serialize();
        auto msg = messaging::MessageBuilder{}
            .from(service_id_)
            .type(AppMessageTypes::AppUnregistrationEvent)
            .payload(std::move(payload))
            .build();
        pending_outbound_.push_back(std::move(msg));
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
    using request = protoflow::app_registration_protocol::request;
    switch (static_cast<request>(msg.type())) {

        case request::hello:
            write(messaging::MessageBuilder{}
                .from(service_id_)
                .type(response::hello_ack)
                .timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count())
                .build());
            break;

        case request::register_app:
            std::cout << "[DEBUG] AppRegistrationService: Received register_app message\n";
            PROTOFLOW_LOG_INFO(*this, "Received register_app message");
            if (register_app(AppRegistrationEvent::deserialize(msg.bytes()))) {
                std::cout << "[DEBUG] AppRegistrationService: Successfully registered app from RPC\n";
                PROTOFLOW_LOG_INFO(*this, "Successfully registered app from RPC");
            } else {
                std::cout << "[DEBUG] AppRegistrationService: Failed to deserialize AppRegistrationEvent\n";
                PROTOFLOW_LOG_WARN(*this, "Failed to deserialize AppRegistrationEvent from message");
            }
            break;

        case request::heartbeat:
            write(messaging::MessageBuilder{}
                .from(service_id_)
                .type(response::heartbeat_ack)
                .timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count())
                .build());
            break;
        case request::unregister_app:
            if (auto ev = AppUnregistrationEvent::deserialize(msg.bytes())) {
                PROTOFLOW_LOG_INFO(*this, "Unregister request for: " << ev->app_name);
                unregister_app(ev->app_name);
            } else {
                PROTOFLOW_LOG_WARN(*this, "Failed to deserialize AppUnregistrationEvent from message");
            }
            break;
        case request::error:
            PROTOFLOW_LOG_WARN(*this, "Received unsupported message type: " << msg.type());
            break;


        // case  {
        //     if (auto event = AppRegistrationEvent::deserialize(msg.bytes())) {
        //         AppRegistration registration;
        //         registration.name = std::move(event->app_name);
        //         registration.version = std::move(event->version);
        //         registration.endpoints = std::move(event->endpoints);
        //         register_app(std::move(registration));
        //     }
        //     break;
        // }
        
        // case MessageTypes::AppUnregistrationEvent: {
        //     if (auto event = AppUnregistrationEvent::deserialize(msg.bytes())) {
        //         unregister_app(event->app_name);
        //     }
        //     break;
        // }
        
        // default:
        //     // Unknown message type, ignore
        //     break;
    }
}

std::vector<messaging::Message> AppRegistrationService::generate_outbound() {
    // Return pending registration/unregistration events to broadcast to HTTPService and other subscribers
    auto messages = std::move(pending_outbound_);
    pending_outbound_.clear();
    return messages;
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
