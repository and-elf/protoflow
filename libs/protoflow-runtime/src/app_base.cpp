#include <protoflow/runtime/app_base.hpp>
#include <protoflow/service/service.hpp>
#include <protoflow/app_registration_client/app_registration_client.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <protoflow/messaging/message.hpp>
#include <protoflow/logging/logging.hpp>
#include <iostream>
#include <thread>
#include <csignal>

namespace protoflow::runtime {

// Thread-local storage for the current app instance
thread_local AppBase* g_app_instance = nullptr;

void AppBase::signal_handler_impl(int signal) {
    if (auto* app = g_app_instance; app != nullptr) {
        std::cout << "\nReceived signal " << signal << ", shutting down...\n";
        app->on_signal(signal);
    }
}

AppBase::AppBase(Config config)
    : config_(std::move(config)), g_instance_(this) {
    g_app_instance = this;
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
    std::cout << "Application started (main loop running)\n";

    while (running_.load()) {
        auto cycle_start = std::chrono::steady_clock::now();

        cycle();

        auto elapsed = std::chrono::steady_clock::now() - cycle_start;
        if (elapsed < config_.cycle_time) {
            std::this_thread::sleep_for(config_.cycle_time - elapsed);
        }
    }

    std::cout << "Application stopped.\n";
}

void AppBase::shutdown() noexcept {
    std::cout << "Shutting down application...\n";
    running_.store(false);

    // Stop all services
    for (auto& service : services_) {
        service->stop();
    }

    std::cout << "All services stopped.\n";
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
                    interested.push_back(dest.get());
                }
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

    // Also handle RPC transport I/O if configured (bridges external transport to services)
    route_rpc_messages();
}

void AppBase::route_rpc_messages() {
    if (!rpc_transport_) {
        return;
    }

    // Find registration client in services
    app_registration_client::AppRegistrationClient* reg_client = nullptr;
    for (auto& service : services_) {
        if (auto* client = dynamic_cast<app_registration_client::AppRegistrationClient*>(service.get())) {
            reg_client = client;
            break;
        }
    }

    if (!reg_client) {
        return;
    }

    auto* transport = static_cast<protoflow::rpc::transport_interface*>(rpc_transport_);

    // Send registration client outbound messages over RPC transport
    while (auto msg = reg_client->pop_outbound()) {
        if (transport->is_connected()) {
            transport->send(std::span<const std::byte>(msg->data));
        }
    }

    // Receive data from RPC transport and forward to registration client
    if (transport->is_connected()) {
        auto result = transport->receive(8192);
        if (result.has_value() && !result->empty()) {
            protoflow::messaging::MessageHeader header;
            header.type = protoflow::messaging::MessageTypes::Payload;
            protoflow::messaging::Message msg{header, std::move(result.value())};
            reg_client->on_message(std::move(msg));
        }
    }
}

void AppBase::setup_rpc_transport(std::unique_ptr<protoflow::rpc::transport_interface> transport,
                                   std::string_view server_desc) noexcept {
    if (!transport) {
        std::cerr << "[WARN]   RPC transport is null\n";
        return;
    }

    std::string desc_str = server_desc.empty() ? "RPC server" : std::string(server_desc);
    
    if (transport->is_connected()) {
        std::cerr << "[INFO]   Connected to " << desc_str << "\n";
    } else {
        std::cerr << "[ERROR]  Failed to connect to " << desc_str << "\n";
    }
    
    set_rpc_transport(std::move(transport));
}

void AppBase::on_signal(int signal) {
    (void)signal;  // Suppress unused parameter warning
    shutdown();
}

logging::LoggingService* AppBase::setup_logging() noexcept {
    auto logging_service = std::make_unique<logging::LoggingService>();
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
