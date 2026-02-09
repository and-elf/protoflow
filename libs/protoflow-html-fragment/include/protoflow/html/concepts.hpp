#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>

namespace protoflow::html {

// Forward declarations
struct node;
template <std::size_t N>
struct fixed_string;

// HTML node concept - any element or text node
template <typename T>
concept html_node = std::derived_from<T, node>;

// HTML attribute concept
template <typename T>
concept html_attr = requires {
    { T::key } -> std::convertible_to<std::string_view>;
    { T::value } -> std::convertible_to<std::string_view>;
};

}  // namespace protoflow::html
