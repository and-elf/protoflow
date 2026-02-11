#pragma once

// App-local convenience header that re-exports specific html-fragment headers
// Avoid including <protoflow/html.hpp> here to prevent recursive include
// resolution issues when the app include directory is searched first.

#include <protoflow/html/elements.hpp>
#include <protoflow/html/fragment.hpp>
#include <protoflow/html/render.hpp>
#include <protoflow/html/text.hpp>
#include <protoflow/html/tag.hpp>
#include <protoflow/html/attrs.hpp>
#include <protoflow/html/render_ctx.hpp>