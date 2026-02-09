// Copyright (c) 2026 Andreas
// SPDX-License-Identifier: MIT
#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace protoflow::http::json {

// Re-export nlohmann::json for convenience
using nlohmann::json;

// Convert any type to JSON string using nlohmann::json
template<typename T>
auto to_json(const T& value) -> std::string {
    return json(value).dump();
}

// Specialization for std::optional - converts to value or null
template<typename T>
auto to_json(const std::optional<T>& value) -> std::string {
    if (value.has_value()) {
        return json(value.value()).dump();
    }
    return "null";
}

} // namespace protoflow::http::json

// nlohmann/json's macro NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE is available for defining JSON serialization
// Usage: NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MyStruct, field1, field2, field3)
