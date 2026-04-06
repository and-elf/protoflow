#pragma once

#include <protoflow/messaging/message.hpp>
#include <memory>
#include <atomic>
#include <chrono>
#include <vector>
#include <cstddef>
#include <span>

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
        std::chrono::milliseconds cycle_time{10};
        
        /// Maximum number of messages to route per cycle
        std::size_t max_messages_per_cycle{100};
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

    /// Get RPC transport (may be null if not configured)
    protoflow::rpc::transport_interface* get_rpc_transport() const noexcept {
        return static_cast<protoflow::rpc::transport_interface*>(rpc_transport_);
    }

    /// Set RPC transport (called by subclasses during initialize)
    void set_rpc_transport(std::unique_ptr<protoflow::rpc::transport_interface> transport) noexcept {
        rpc_transport_ = transport.release();
    }

    /// Setup RPC transport with connection and logging
    /// Handles connect attempt, logs result, and stores transport
    /// \param transport The transport to connect and manage
    /// \param server_desc Optional description for logging (e.g., "RPC server at 127.0.0.1:9999")
    void setup_rpc_transport(std::unique_ptr<protoflow::rpc::transport_interface> transport,
                             std::string_view server_desc = "") noexcept;

    /// Get logging service (for apps that need direct access)
    [[nodiscard]] logging::LoggingService* get_logger() const noexcept {
        return logger_;
    }

    /// Initialize logging service (call at start of app's initialize())
    /// Creates LoggingService and adds it to services_, returns pointer for optional configuration
    logging::LoggingService* setup_logging() noexcept;

    /// Unified logging methods (route to logger service if available)
    void log_trace(const std::string& msg) noexcept;
    void log_debug(const std::string& msg) noexcept;
    void log_info(const std::string& msg) noexcept;
    void log_warn(const std::string& msg) noexcept;
    void log_error(const std::string& msg) noexcept;
    void log_fatal(const std::string& msg) noexcept;

    std::vector<std::unique_ptr<service::Service>> services_;

private:
    /// Global signal handler (static bridge to instance method)
    static void signal_handler_impl(int signal);

    /// Instance-level signal handling
    void on_signal(int signal);

    Config config_;
    std::atomic<bool> running_{false};
    std::atomic<AppBase*> g_instance_{nullptr};

    // RPC transport for connecting to main app (optional, owned by AppBase)
    void* rpc_transport_{nullptr};

    // Logging service (owned by AppBase, stored in services_)
    logging::LoggingService* logger_{nullptr};

    // Helper functions for message routing
    void route_rpc_messages();
};

} // namespace protoflow::runtime
