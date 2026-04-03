#pragma once

#include <protoflow/messaging/message.hpp>
#include <protoflow/app_registration_protocol/messages.hpp>
#include <string>
#include <optional>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <span>

namespace protoflow::mainapp {

/// Message types for intra-service HTTP request/response events.
/// The HttpListenerService serializes incoming TCP data into these events
/// and the HTTPService consumes/produces them purely via the message bus.
namespace HttpMessageTypes {
    constexpr protoflow::messaging::MessageType HttpRequest  = 200;
    constexpr protoflow::messaging::MessageType HttpResponse = 201;
}

/// Lightweight HTTP request event carried over the message bus.
/// Serialized as: connection_id(8) | method_len(4) | method | path_len(4) | path
///                | header_count(4) | [key_len(4) key val_len(4) val]...
///                | body_len(4) | body
struct HttpRequestEvent {
    uint64_t connection_id{0};
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::vector<std::byte> body;

    [[nodiscard]] std::vector<std::byte> serialize() const {
        std::vector<std::byte> out;
        auto push = [&](const void* p, size_t n) {
            auto old = out.size();
            out.resize(old + n);
            std::memcpy(out.data() + old, p, n);
        };
        auto push_str = [&](const std::string& s) {
            auto len = static_cast<uint32_t>(s.size());
            push(&len, 4);
            push(s.data(), s.size());
        };
        push(&connection_id, 8);
        push_str(method);
        push_str(path);
        auto hdr_count = static_cast<uint32_t>(headers.size());
        push(&hdr_count, 4);
        for (const auto& [k, v] : headers) { push_str(k); push_str(v); }
        auto body_len = static_cast<uint32_t>(body.size());
        push(&body_len, 4);
        push(body.data(), body.size());
        return out;
    }

    static std::optional<HttpRequestEvent> deserialize(std::span<const std::byte> d) {
        size_t off = 0;
        auto remaining = [&]() -> size_t { return d.size() > off ? d.size() - off : 0; };
        auto read_u32 = [&](uint32_t& v) -> bool {
            if (remaining() < 4) return false;
            std::memcpy(&v, d.data() + off, 4); off += 4; return true;
        };
        auto read_u64 = [&](uint64_t& v) -> bool {
            if (remaining() < 8) return false;
            std::memcpy(&v, d.data() + off, 8); off += 8; return true;
        };
        auto read_str = [&](std::string& s) -> bool {
            uint32_t len; if (!read_u32(len)) return false;
            if (remaining() < len) return false;
            s.assign(reinterpret_cast<const char*>(d.data() + off), len); off += len;
            return true;
        };

        HttpRequestEvent ev;
        if (!read_u64(ev.connection_id)) return std::nullopt;
        if (!read_str(ev.method))        return std::nullopt;
        if (!read_str(ev.path))          return std::nullopt;
        uint32_t hdr_count;
        if (!read_u32(hdr_count)) return std::nullopt;
        for (uint32_t i = 0; i < hdr_count; ++i) {
            std::string k, v;
            if (!read_str(k) || !read_str(v)) return std::nullopt;
            ev.headers[std::move(k)] = std::move(v);
        }
        uint32_t body_len;
        if (!read_u32(body_len)) return std::nullopt;
        if (remaining() < body_len) return std::nullopt;
        ev.body.assign(d.begin() + static_cast<ptrdiff_t>(off),
                       d.begin() + static_cast<ptrdiff_t>(off + body_len));
        return ev;
    }
};

/// Lightweight HTTP response event carried over the message bus.
/// Serialized as: connection_id(8) | status_code(4)
///                | header_count(4) | [key_len(4) key val_len(4) val]...
///                | body_len(4) | body
struct HttpResponseEvent {
    uint64_t connection_id{0};
    int32_t  status_code{200};
    std::unordered_map<std::string, std::string> headers;
    std::vector<std::byte> body;

    [[nodiscard]] std::vector<std::byte> serialize() const {
        std::vector<std::byte> out;
        auto push = [&](const void* p, size_t n) {
            auto old = out.size();
            out.resize(old + n);
            std::memcpy(out.data() + old, p, n);
        };
        auto push_str = [&](const std::string& s) {
            auto len = static_cast<uint32_t>(s.size());
            push(&len, 4);
            push(s.data(), s.size());
        };
        push(&connection_id, 8);
        push(&status_code, 4);
        auto hdr_count = static_cast<uint32_t>(headers.size());
        push(&hdr_count, 4);
        for (const auto& [k, v] : headers) { push_str(k); push_str(v); }
        auto body_len = static_cast<uint32_t>(body.size());
        push(&body_len, 4);
        push(body.data(), body.size());
        return out;
    }

    static std::optional<HttpResponseEvent> deserialize(std::span<const std::byte> d) {
        size_t off = 0;
        auto remaining = [&]() -> size_t { return d.size() > off ? d.size() - off : 0; };
        auto read_u32 = [&](uint32_t& v) -> bool {
            if (remaining() < 4) return false;
            std::memcpy(&v, d.data() + off, 4); off += 4; return true;
        };
        auto read_u64 = [&](uint64_t& v) -> bool {
            if (remaining() < 8) return false;
            std::memcpy(&v, d.data() + off, 8); off += 8; return true;
        };
        auto read_str = [&](std::string& s) -> bool {
            uint32_t len; if (!read_u32(len)) return false;
            if (remaining() < len) return false;
            s.assign(reinterpret_cast<const char*>(d.data() + off), len); off += len;
            return true;
        };

        HttpResponseEvent ev;
        if (!read_u64(ev.connection_id)) return std::nullopt;
        uint32_t sc;
        if (!read_u32(sc)) return std::nullopt;
        ev.status_code = static_cast<int32_t>(sc);
        uint32_t hdr_count;
        if (!read_u32(hdr_count)) return std::nullopt;
        for (uint32_t i = 0; i < hdr_count; ++i) {
            std::string k, v;
            if (!read_str(k) || !read_str(v)) return std::nullopt;
            ev.headers[std::move(k)] = std::move(v);
        }
        uint32_t body_len;
        if (!read_u32(body_len)) return std::nullopt;
        if (remaining() < body_len) return std::nullopt;
        ev.body.assign(d.begin() + static_cast<ptrdiff_t>(off),
                       d.begin() + static_cast<ptrdiff_t>(off + body_len));
        return ev;
    }
};


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
