#pragma once

#include <protoflow/messaging/message.hpp>
#include <memory>
#include <atomic>
#include <chrono>
#include <vector>
#include <cstddef>
#include <span>
#include <optional>
#include <functional>

namespace protoflow::config {
struct LoggingConfig;
}

namespace protoflow::logging {
class LoggingService;
}

namespace protoflow::service {
class Service;
}

namespace protoflow::rpc {
class transport_interface;
}

namespace protoflow::app_registration_client {
class AppRegistrationClient;
}

namespace protoflow::runtime {

/// Base class for protoflow applications
///
/// Provides built-in lifecycle management including:
/// - Signal handling (SIGINT, SIGTERM) for graceful shutdown
/// - Main loop execution with configurable cycle timing
/// - Message routing framework
/// - Virtual methods for app customization
///
/// Apps should:
/// 1. Inherit from AppBase
/// 2. Override initialize() to set up services
/// 3. Optionally override cycle() to add custom logic
/// 4. Optionally override route_messages() for custom routing
/// 5. Call run() from main() to start the app
///
/// Example:
/// ```cpp
/// class MyApp : public AppBase {
/// public:
///     bool initialize() override {
///         // Create and register services
///         services_.push_back(std::make_unique<MyService>());
///         return true;
///     }
///
///     void cycle() override {
///         AppBase::cycle();  // Call base implementation
///         // Add custom cycle logic here
///     }
/// };
///
/// int main() {
///     MyApp app(AppBase::Config{.cycle_time = std::chrono::milliseconds(10)});
///     if (!app.initialize()) return 1;
///     app.run();
///     return 0;
/// }
/// ```
class AppBase {
public:
    struct Config {
        /// Main loop cycle time
        const std::chrono::milliseconds cycle_time{10};
        
        /// Maximum number of messages to route per cycle
        const std::size_t max_messages_per_cycle{100};

        // Application name for logging and registration (optional, can be set in registration config)
        const std::string app_name;
    };

    explicit AppBase(Config config);
    explicit AppBase() : AppBase(Config{}) {}
    virtual ~AppBase();

    // Non-copyable
    AppBase(const AppBase&) = delete;
    AppBase& operator=(const AppBase&) = delete;

    /// Initialize the application (must be called before run())
    /// Override in subclasses to set up services and state
    virtual bool initialize() = 0;

    /// Start the main run loop (blocks until shutdown)
    /// Handles signal setup and teardown automatically
    void run();

    /// Request graceful shutdown (can be called from signal handler)
    void shutdown() noexcept;

    /// Check if the application is currently running
    [[nodiscard]] bool is_running() const noexcept {
        return running_.load();
    }

protected:
    /// Execute one cycle of the main loop
    /// Default implementation:
    /// 1. Polls all services (including RPC transport polling)
    /// 2. Routes messages between services and RPC transport
    /// 3. Calls route_messages() for app-specific routing
    /// 4. Sleeps if cycle finished early
    /// 
    /// Override to add custom behavior, but call AppBase::cycle() first
    virtual void cycle();

    /// Route messages between services
    /// Default implementation handles AppRegistrationClient routing to RPC transport
    /// Override in subclasses to add app-specific routing
    virtual void route_messages();

    /// Access to services list (for subclasses)
    std::vector<std::unique_ptr<service::Service>>& get_services() noexcept {
        return services_;
    }

    const std::vector<std::unique_ptr<service::Service>>& get_services() const noexcept {
        return services_;
    }

    /// Get the built-in app registration client service
    /// Returns optional reference to the client; should always have a value if AppBase was properly constructed
    [[nodiscard]] std::optional<std::reference_wrapper<app_registration_client::AppRegistrationClient>> get_app_registration_client() noexcept;

    /// Configure app registration client
    /// Replaces the placeholder app registration client with one fully configured
    /// with the specified parameters. The RPC transport layer is managed separately.
    /// 
    /// \param app_name Application name for registration (must not be empty)
    /// \param endpoints RPC endpoints this app provides (can be empty)
    /// \return true if configuration succeeded
    /// 
    /// Example:
    /// ```cpp
    /// configure_app_registration("my-app", {"endpoint1", "endpoint2"});
    /// 
    /// // Separately, set up transport:
    /// auto tcp_transport = std::make_unique<MyTransport>("localhost", 9000);
    /// if (tcp_transport->connect()) {
    ///     // Transport is now ready for RPC communication
    /// }
    /// ```
    bool configure_app_registration(std::string_view app_name,
                                   std::span<const std::string> endpoints = {}) noexcept;

    /// Get logging service (for apps that need direct access)
    [[nodiscard]] logging::LoggingService* get_logger() const noexcept {
        return logger_;
    }

    /// Initialize logging service (call at start of app's initialize())
    /// Creates LoggingService, applies configuration, adds it to services_, returns pointer
    /// \param config Logging configuration (min level, console output, max stored logs)
    logging::LoggingService* setup_logging(const config::LoggingConfig& config) noexcept;

    /// Unified logging methods (route to logger service if available)
    void log_trace(const std::string& msg) noexcept;
    void log_debug(const std::string& msg) noexcept;
    void log_info(const std::string& msg) noexcept;
    void log_warn(const std::string& msg) noexcept;
    void log_error(const std::string& msg) noexcept;
    void log_fatal(const std::string& msg) noexcept;

    std::vector<std::unique_ptr<service::Service>> services_;

    /// Initialize and register the built-in app registration client
    /// Called once during AppBase construction to set up the service
    void init_app_registration_client() noexcept;

private:
    /// Global signal handler (static bridge to instance method)
    static void signal_handler_impl(int signal);

    /// Instance-level signal handling
    void on_signal(int signal);

    Config config_;
    std::atomic<bool> running_{false};
    std::atomic<AppBase*> g_instance_{nullptr};

    // Logging service (owned by AppBase, stored in services_)
    logging::LoggingService* logger_{nullptr};
};

} // namespace protoflow::runtime
