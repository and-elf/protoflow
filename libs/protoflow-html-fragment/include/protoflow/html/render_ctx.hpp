#pragma once

#include <string>
#include <string_view>

namespace protoflow::html {

// Rendering context for building HTML strings
class render_ctx {
public:
    render_ctx() = default;

    void write(std::string_view sv) {
        output_.append(sv);
    }

    void write(const char* str) {
        output_.append(str);
    }

    void write(char c) {
        output_.push_back(c);
    }

    std::string result() const {
        return output_;
    }

    std::string&& take_result() {
        return std::move(output_);
    }

private:
    std::string output_;
};

}  // namespace protoflow::html
