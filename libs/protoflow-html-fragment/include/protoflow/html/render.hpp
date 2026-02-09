#pragma once

#include "node.hpp"
#include "render_ctx.hpp"
#include <ostream>
#include <string>

namespace protoflow::html {

// Render a tag to a string
template <typename Tag>
std::string to_html(const Tag& tag) {
    render_ctx ctx;
    tag.render(ctx);
    return ctx.take_result();
}

// Render a tag to an output stream
template <typename Tag>
void render(std::ostream& os, const Tag& tag) {
    std::string html = to_html(tag);
    os << html;
}

}  // namespace protoflow::html
