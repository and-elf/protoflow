#pragma once

#include <protoflow/messaging/message.hpp>
#include <memory>
#include <atomic>
#include <chrono>
#include <vector>
#include <cstddef>
#include <span>

namespace protoflow::service {
class Service;
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
    /// 1. Polls all services
    /// 2. Calls route_messages()
    /// 3. Sleeps if cycle finished early
    /// 
    /// Override to add custom behavior, but call AppBase::cycle() first
    virtual void cycle();

    /// Route messages between services
    /// Default implementation is empty - override in subclasses
    virtual void route_messages();

    /// Access to services list (for subclasses)
    std::vector<std::unique_ptr<service::Service>>& get_services() noexcept {
        return services_;
    }

    const std::vector<std::unique_ptr<service::Service>>& get_services() const noexcept {
        return services_;
    }

private:
    /// Global signal handler (static bridge to instance method)
    static void signal_handler_impl(int signal);

    /// Instance-level signal handling
    void on_signal(int signal);

    Config config_;
    std::atomic<bool> running_{false};
    std::atomic<AppBase*> g_instance_{nullptr};

protected:
    std::vector<std::unique_ptr<service::Service>> services_;
};

} // namespace protoflow::runtime
