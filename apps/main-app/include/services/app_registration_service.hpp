#pragma once

#include <protoflow/service.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <memory>

namespace protoflow::mainapp {

/// Application metadata
struct AppRegistration {
    std::string name;
    std::string version;
    std::vector<std::string> endpoints;
    std::vector<std::string> hw_requirements;
    std::chrono::steady_clock::time_point last_keepalive;
    bool active = false;
};

/// Service managing registered applications
/// Handles app lifecycle and state tracking via messaging
class AppRegistrationService : public service::Service {
public:
    AppRegistrationService();
    ~AppRegistrationService() override;

    void start() override;
    void stop() override;
    void poll() override;

    /// Register a new application
    bool register_app(const AppRegistration& registration);

    /// Unregister an application
    void unregister_app(const std::string& name);

    /// Update keepalive timestamp for an app
    void update_keepalive(const std::string& name);

    /// Get all registered apps
    [[nodiscard]] std::vector<AppRegistration> get_registered_apps() const;

    /// Get specific app by name
    [[nodiscard]] std::optional<AppRegistration> get_app(const std::string& name) const;

    /// Aggregate state from all registered apps as JSON
    [[nodiscard]] std::string aggregate_state_json() const;

protected:
    void handle(messaging::Message&& msg) override;
    std::vector<messaging::Message> generate_outbound() override;

private:
    /// Check for stale connections (no keepalive)
    void check_keepalives();

    std::unordered_map<std::string, AppRegistration> registered_apps_;
    std::chrono::seconds keepalive_timeout_{30};
    std::chrono::steady_clock::time_point last_check_;
};

} // namespace protoflow::mainapp
