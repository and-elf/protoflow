#pragma once

#include "fixed_string.hpp"
#include <string_view>

namespace protoflow::html {

// Attribute pack - empty template for variadic attributes
template <typename... A>
struct attrs {};

// Generic attribute definition
template <fixed_string Key, fixed_string Value>
struct attr {
    static constexpr std::string_view key = Key.view();
    static constexpr std::string_view value = Value.view();
};

// Common attribute types
template <fixed_string Value>
struct class_ {
    static constexpr std::string_view key = "class";
    static constexpr std::string_view value = Value.view();
};

template <fixed_string Value>
struct id_ {
    static constexpr std::string_view key = "id";
    static constexpr std::string_view value = Value.view();
};

template <fixed_string Value>
struct href_ {
    static constexpr std::string_view key = "href";
    static constexpr std::string_view value = Value.view();
};

template <fixed_string Value>
struct style_ {
    static constexpr std::string_view key = "style";
    static constexpr std::string_view value = Value.view();
};

template <fixed_string Value>
struct src_ {
    static constexpr std::string_view key = "src";
    static constexpr std::string_view value = Value.view();
};

template <fixed_string Value>
struct alt_ {
    static constexpr std::string_view key = "alt";
    static constexpr std::string_view value = Value.view();
};

template <fixed_string Value>
struct title_ {
    static constexpr std::string_view key = "title";
    static constexpr std::string_view value = Value.view();
};

template <fixed_string Value>
struct type_ {
    static constexpr std::string_view key = "type";
    static constexpr std::string_view value = Value.view();
};

template <fixed_string Value>
struct name_ {
    static constexpr std::string_view key = "name";
    static constexpr std::string_view value = Value.view();
};

template <fixed_string Value>
struct value_ {
    static constexpr std::string_view key = "value";
    static constexpr std::string_view value = Value.view();
};

template <fixed_string Value>
struct placeholder_ {
    static constexpr std::string_view key = "placeholder";
    static constexpr std::string_view value = Value.view();
};

}  // namespace protoflow::html
