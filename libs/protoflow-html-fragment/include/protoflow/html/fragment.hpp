#pragma once

#include "node.hpp"
#include "render_ctx.hpp"
#include <memory>
#include <string>

namespace protoflow::html {

// Fragment interface for polymorphic HTML rendering
class fragment {
public:
    virtual ~fragment() = default;
    virtual std::string render() const = 0;
};

// Fragment implementation wrapping a tag
template <typename Tag>
class fragment_impl : public fragment {
public:
    explicit fragment_impl(Tag tag) : tag_(std::move(tag)) {}

    std::string render() const override {
        render_ctx ctx;
        tag_.render(ctx);
        return ctx.take_result();
    }

private:
    Tag tag_;
};

// Helper to create a fragment from a tag
template <typename Tag>
std::unique_ptr<fragment> make_fragment(Tag tag) {
    return std::make_unique<fragment_impl<Tag>>(std::move(tag));
}

}  // namespace protoflow::html
