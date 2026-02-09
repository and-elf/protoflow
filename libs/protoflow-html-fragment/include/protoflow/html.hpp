#pragma once

// Main header for protoflow-html-fragment library
// Compile-time HTML generation using C++23 concepts

#include "html/attrs.hpp"
#include "html/concepts.hpp"
#include "html/elements.hpp"
#include "html/fixed_string.hpp"
#include "html/fragment.hpp"
#include "html/node.hpp"
#include "html/render.hpp"
#include "html/render_ctx.hpp"
#include "html/tag.hpp"
#include "html/text.hpp"
#include "html/components.hpp"

namespace protoflow {
// Re-export html namespace for convenience
using namespace html;
}
