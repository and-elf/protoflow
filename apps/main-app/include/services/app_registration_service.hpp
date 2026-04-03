#pragma once

#include <protoflow/service.hpp>
#include <protoflow/messages.hpp>
#include <protoflow/app_registration_protocol/messages.hpp>
#include <protoflow/logging/macros.hpp>
#include <app_state_machine.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <memory>

namespace protoflow::mainapp {

/// Sink for observing app state transitions
/// Passed to FSM to provide logging/tracing without coupling
struct AppStateSink {
    std::string app_name;
    
    void on_transition(AppState from, AppEvent event, AppState to) {
        // Log state transitions for observability
        // In production, you'd use a proper logger here
        (void)from; (void)event; (void)to; // Suppress unused warnings
    }
    
    void on_invalid(AppState state, AppEvent event) {
        // Log rejected events - required by FSM table library
        (void)state; (void)event; // Suppress unused warnings
    }
};

/// Application metadata with FSM-based lifecycle management
struct AppRegistration {
    std::string name;
    std::vector<std::string> endpoints;
    std::vector<std::string> hw_requirements;
    std::chrono::steady_clock::time_point last_keepalive;
    std::unique_ptr<AppStateMachine<AppStateSink>> fsm;
};

/// Service managing registered applications
/// Handles app lifecycle and state tracking via messaging
class AppRegistrationService : public service::Service {
public:
    explicit AppRegistrationService(std::chrono::milliseconds check_interval = std::chrono::milliseconds(5000));
    ~AppRegistrationService() override;

    void start() override;
    void stop() override;
    void poll() override;
    
    /// Get specific app by name (returns optional reference to avoid copy)
    [[nodiscard]] std::optional<std::reference_wrapper<const AppRegistration>> get_app(const std::string& name) const;

protected:
    void handle(messaging::Message&& msg) override;
    std::vector<messaging::Message> generate_outbound() override;
    
private:
    /// Get all registered apps (returns pointers to avoid copy)
    [[nodiscard]] std::vector<const AppRegistration*> get_registered_apps() const;
    /// Register a new application (takes ownership via move)
    bool register_app(AppRegistration&& registration);

    bool register_app(std::optional<AppRegistrationEvent> event);


    /// Unregister an application
    void unregister_app(const std::string& name);

    /// Update keepalive timestamp for an app
    void update_keepalive(const std::string& name);

    /// Get specific app by name (mutable version for internal use)
    [[nodiscard]] std::optional<std::reference_wrapper<AppRegistration>> get_app(const std::string& name);

    /// Aggregate state from all registered apps as JSON
    [[nodiscard]] std::string aggregate_state_json() const;

    /// Check for stale connections (no keepalive)
    void check_keepalives();

    std::unordered_map<std::string, AppRegistration> registered_apps_;
    std::chrono::seconds keepalive_timeout_{30};
    std::chrono::milliseconds check_interval_{5000};
    std::chrono::steady_clock::time_point last_check_;
};

} // namespace protoflow::mainapp
