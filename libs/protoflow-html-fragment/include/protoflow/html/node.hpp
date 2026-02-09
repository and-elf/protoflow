#pragma once

#include "render_ctx.hpp"

namespace protoflow::html {

// Base class for all HTML nodes
struct node {
    virtual ~node() = default;
    virtual void render(render_ctx& ctx) const = 0;
};

}  // namespace protoflow::html
