#include <protoflow/runtime/app_base.hpp>
#include <protoflow/service/service.hpp>
#include <protoflow/app_registration_client/app_registration_client.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <protoflow/messaging/message.hpp>
#include <protoflow/logging/logging.hpp>
#include <protoflow/config/logging_config.hpp>
#include <iostream>
#include <thread>
#include <csignal>
#include <cstdlib>

namespace protoflow::runtime {

// Thread-local storage for the current app instance
thread_local AppBase* g_app_instance = nullptr;

void AppBase::signal_handler_impl(int signal) {
    if (auto* app = g_app_instance; app != nullptr) {
        app->log_info("\nReceived signal " + std::to_string(signal) + ", shutting down...\n");
        app->on_signal(signal);
    }
}

AppBase::AppBase(Config config)
    : config_(std::move(config)), g_instance_(this) {
    g_app_instance = this;

    init_app_registration_client();
}

AppBase::~AppBase() {
    if (running_.load()) {
        shutdown();
    }
    g_app_instance = nullptr;
    g_instance_.store(nullptr);
}

void AppBase::run() {
    // Register signal handlers
    std::signal(SIGINT, signal_handler_impl);
    std::signal(SIGTERM, signal_handler_impl);

    for (auto& service : services_) {
        service->start();
    }

    running_.store(true);
    log_info("Application started (main loop running)\n");

    while (running_.load()) {
        auto cycle_start = std::chrono::steady_clock::now();

        cycle();

        auto elapsed = std::chrono::steady_clock::now() - cycle_start;
        if (elapsed < config_.cycle_time) {
            std::this_thread::sleep_for(config_.cycle_time - elapsed);
        }
    }

    log_info("Application stopped.\n");
}

void AppBase::shutdown() noexcept {
    log_info("Shutting down application...\n");
    running_.store(false);

    // Stop all services
    for (auto& service : services_) {
        service->stop();
    }

    log_info("All services stopped.\n");
}

void AppBase::cycle() {
    // Poll all services
    for (auto& service : services_) {
        service->poll();
    }

    // Route messages (including RPC<->registration client routing)
    route_messages();
}

void AppBase::route_messages() {
    // Generic service-to-service routing based on message type subscriptions
    for (auto& source : services_) {
        while (auto opt = source->pop_outbound()) {
            auto msg = std::move(*opt);
            
            std::cerr << "[ROUTE_DEBUG] Message type=" << int(msg.type()) << "\n";
            
            // Find all services interested in this message type
            std::vector<service::Service*> interested;
            for (auto& dest : services_) {
                // Skip source service
                if (dest.get() == source.get()) continue;

                auto types = dest->get_message_types();
                
                // Empty subscription = wildcard (subscribe to all)
                // Or check if subscribed to this specific message type
                bool wants_it = types.empty();
                if (!wants_it) {
                    for (auto t : types) {
                        if (t == msg.type()) {
                            wants_it = true;
                            break;
                        }
                    }
                }
                
                if (wants_it) {
                    std::cerr << "[ROUTE_DEBUG]   -> Routing to interested service\n";
                    interested.push_back(dest.get());
                }
            }
            
            if (interested.empty()) {
                std::cerr << "[ROUTE_DEBUG]   -> No interested services\n";
            }
            
            // Deliver to all interested services (copy except last, which gets moved)
            for (size_t i = 0; i < interested.size(); ++i) {
                if (i + 1 < interested.size()) {
                    // Copy for all but the last
                    interested[i]->on_message(messaging::Message(msg));
                } else {
                    // Move for the last one
                    interested[i]->on_message(std::move(msg));
                }
            }
        }
    }
}

void AppBase::on_signal(int signal) {
    (void)signal;  // Suppress unused parameter warning
    shutdown();
}

void AppBase::init_app_registration_client() noexcept {
    // Create a minimal app registration client with default config
    // The app will call configure_app_registration() to fully configure it
    app_registration_client::Config config;
    config.app_name = config_.app_name;
    auto client = std::make_unique<app_registration_client::AppRegistrationClient>(config);
    
    services_.push_back(std::move(client));
}

std::optional<std::reference_wrapper<app_registration_client::AppRegistrationClient>> AppBase::get_app_registration_client() noexcept {
    // Find the app registration client in services_
    for (auto& svc : services_) {
        if (auto* client = dynamic_cast<app_registration_client::AppRegistrationClient*>(svc.get())) {
            return std::ref(*client);
        }
    }
    return std::nullopt;
}

bool AppBase::configure_app_registration(std::string_view app_name,
                                         std::span<const std::string> endpoints) noexcept {
    if (app_name.empty()) {
        log_error("Invalid parameters: app_name must not be empty");
        return false;
    }

    // Create new app registration client config with proper parameters
    app_registration_client::Config reg_config;
    reg_config.app_name = std::string(app_name);
    reg_config.version = 1;
    
    // Copy endpoints
    reg_config.endpoints.assign(endpoints.begin(), endpoints.end());
    
    // Create new client with the full config
    auto new_client = std::make_unique<app_registration_client::AppRegistrationClient>(
        std::move(reg_config));
    
    // Find and replace the old placeholder client in services_
    for (auto& svc : services_) {
        if (dynamic_cast<app_registration_client::AppRegistrationClient*>(svc.get())) {
            svc = std::move(new_client);
            break;
        }
    }
    
    log_info("App registration configured as '" + std::string(app_name) + "'");
    return true;
}

logging::LoggingService* AppBase::setup_logging(const config::LoggingConfig& log_config) noexcept {
    auto logging_service = std::make_unique<logging::LoggingService>();
    logging_service->set_min_level(log_config.min_level);
    logging_service->set_console_output(log_config.console_output);
    logging_service->set_max_stored_logs(log_config.max_stored_logs);
    
    logger_ = logging_service.get();
    services_.push_back(std::move(logging_service));
    return logger_;
}

void AppBase::log_trace(const std::string& msg) noexcept {
    if (logger_) {
        logger_->log_trace(msg);
    }
}

void AppBase::log_debug(const std::string& msg) noexcept {
    if (logger_) {
        logger_->log_debug(msg);
    }
}

void AppBase::log_info(const std::string& msg) noexcept {
    if (logger_) {
        logger_->log_info(msg);
    }
}

void AppBase::log_warn(const std::string& msg) noexcept {
    if (logger_) {
        logger_->log_warn(msg);
    }
}

void AppBase::log_error(const std::string& msg) noexcept {
    if (logger_) {
        logger_->log_error(msg);
    }
}

void AppBase::log_fatal(const std::string& msg) noexcept {
    if (logger_) {
        logger_->log_fatal(msg);
    }
}

} // namespace protoflow::runtime
