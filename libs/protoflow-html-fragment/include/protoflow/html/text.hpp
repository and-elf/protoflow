#pragma once

#include "node.hpp"
#include <string>
#include <string_view>

namespace protoflow::html {

// Text node
class text : public node {
public:
    text(std::string_view content)
        : content_(content) {}

    text(const char* content)
        : content_(content) {}

    void render(render_ctx& ctx) const override {
        // Simple HTML escaping
        for (char c : content_) {
            switch (c) {
                case '<':
                    ctx.write("&lt;");
                    break;
                case '>':
                    ctx.write("&gt;");
                    break;
                case '&':
                    ctx.write("&amp;");
                    break;
                case '"':
                    ctx.write("&quot;");
                    break;
                case '\'':
                    ctx.write("&#39;");
                    break;
                default:
                    ctx.write(c);
                    break;
            }
        }
    }

private:
    std::string content_;
};

}  // namespace protoflow::html
