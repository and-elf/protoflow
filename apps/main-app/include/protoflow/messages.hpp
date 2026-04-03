#pragma once

#include <protoflow/messaging/message.hpp>
#include <protoflow/app_registration_protocol/messages.hpp>
#include <string>
#include <optional>
#include <vector>
#include <cstring>
#include <span>

namespace protoflow::mainapp {


/// Message: Notify services when an app is registered
/// Reuses app_registration_protocol wire format for consistency
/// Wire format: register_app_msg from protocol library
struct AppRegistrationEvent {
    std::string app_name;
    std::vector<std::string> endpoints;
    
    // Serialize using protocol format
    std::vector<std::byte> serialize() const {
        // Use register_app_msg format
        app_registration_protocol::register_app_msg msg{};
        std::strncpy(msg.name, app_name.c_str(), sizeof(msg.name) - 1);
        msg.endpoint_count = static_cast<uint32_t>(endpoints.size());
        
        std::vector<std::byte> data;
        data.resize(sizeof(msg));
        std::memcpy(data.data(), &msg, sizeof(msg));
        
        // Append endpoint strings (null-terminated)
        for (const auto& ep : endpoints) {
            size_t old_size = data.size();
            data.resize(old_size + ep.size() + 1);
            std::memcpy(data.data() + old_size, ep.c_str(), ep.size() + 1);
        }
        
        return data;
    }
    
    // Deserialize from protocol format
    static std::optional<AppRegistrationEvent> deserialize(std::span<const std::byte> data) {
        if (data.size() < sizeof(app_registration_protocol::register_app_msg)) {
            return std::nullopt;
        }
        
        app_registration_protocol::register_app_msg msg;
        std::memcpy(&msg, data.data(), sizeof(msg));
        
        AppRegistrationEvent event;
        event.app_name = msg.name;
        
        // Parse endpoint strings
        size_t offset = sizeof(msg);
        for (uint32_t i = 0; i < msg.endpoint_count && offset < data.size(); ++i) {
            const char* str = reinterpret_cast<const char*>(data.data() + offset);
            size_t len = strnlen(str, data.size() - offset);
            if (offset + len >= data.size()) break;
            
            event.endpoints.emplace_back(str, len);
            offset += len + 1;
        }
        
        return event;
    }
};

/// Message: Notify services when an app is unregistered
/// Simple wire format: [app_name_len:4][app_name]
struct AppUnregistrationEvent {
    std::string app_name;
    
    std::vector<std::byte> serialize() const {
        std::vector<std::byte> data;
        auto name_len = static_cast<uint32_t>(app_name.size());
        data.resize(4 + name_len);
        std::memcpy(data.data(), &name_len, 4);
        std::memcpy(data.data() + 4, app_name.data(), name_len);
        return data;
    }
    
    static std::optional<AppUnregistrationEvent> deserialize(std::span<const std::byte> data) {
        if (data.size() < 4) return std::nullopt;
        
        uint32_t name_len;
        std::memcpy(&name_len, data.data(), 4);
        if (4 + name_len != data.size()) return std::nullopt;
        
        AppUnregistrationEvent event;
        event.app_name.resize(name_len);
        std::memcpy(event.app_name.data(), data.data() + 4, name_len);
        return event;
    }
};

} // namespace protoflow::mainapp // namespace protoflow::mainapp
