#include <protoflow/runtime/app_base.hpp>
#include <protoflow/service/service.hpp>
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
        if (service) {
            service->stop();
        }
    }

    std::cout << "All services stopped.\n";
}

void AppBase::cycle() {
    // Poll all services
    for (auto& service : services_) {
        if (service) {
            service->poll();
        }
    }

    // Route messages
    route_messages();
}

void AppBase::route_messages() {
    for (auto& src : services_) {
        if (!src) continue;

        while (auto opt = src->pop_outbound()) {
            auto msg = std::move(*opt);

            bool delivered = false;
            for (auto& dst : services_) {
                if (!dst) continue;
                auto types = dst->get_message_types();
                if (types.empty()) continue;

                for (auto t : types) {
                    if (t == msg.type()) {
                        dst->on_message(messaging::Message(msg));
                        delivered = true;
                        break;
                    }
                }
            }
            (void)delivered;
        }
    }
}

void AppBase::on_signal(int signal) {
    (void)signal;  // Suppress unused parameter warning
    shutdown();
}

} // namespace protoflow::runtime
