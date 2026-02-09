#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace protoflow::html {

// Compile-time string type  
template <std::size_t N>
struct fixed_string {
    char data[N + 1]{};  // +1 for null terminator
    
    constexpr fixed_string(const char (&str)[N + 1]) {
        std::copy_n(str, N + 1, data);
    }
    
    constexpr std::string_view view() const {
        return std::string_view(data, N);
    }
    
    constexpr operator std::string_view() const {
        return view();
    }
    
    constexpr const char* c_str() const {
        return data;
    }
    
    static constexpr std::size_t size() {
        return N;
    }
};

// Deduction guide
template <std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N - 1>;

}  // namespace protoflow::html
