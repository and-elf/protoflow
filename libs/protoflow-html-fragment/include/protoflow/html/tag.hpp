#pragma once

#include "attrs.hpp"
#include "concepts.hpp"
#include "node.hpp"
#include "render_ctx.hpp"
#include <string_view>
#include <tuple>
#include <utility>

namespace protoflow::html {

// Helper to extract attributes and children from a type list
template <typename... Types>
struct tag_params {
    std::tuple<Types...> children;
    
    constexpr tag_params(Types... types) : children(std::move(types)...) {}
};

// HTML tag with parameters
template <const char* Name, typename... Params>
struct tag : node {
    tag_params<Params...> params;

    constexpr tag(Params... p) : params(std::move(p)...) {}

    void render(render_ctx& ctx) const override {
        ctx.write("<");
        ctx.write(Name);

        // Write attributes (filter compile-time)
        write_attrs(ctx, std::make_index_sequence<sizeof...(Params)>{});

        ctx.write(">");

        // Render children (filter compile-time)
        render_children(ctx, std::make_index_sequence<sizeof...(Params)>{});

        ctx.write("</");
        ctx.write(Name);
        ctx.write(">");
    }

private:
    template <std::size_t... I>
    void write_attrs(render_ctx& ctx, std::index_sequence<I...>) const {
        (write_attr_if<I>(ctx), ...);
    }

    template <std::size_t I>
    void write_attr_if(render_ctx& ctx) const {
        using T = std::tuple_element_t<I, decltype(params.children)>;
        if constexpr (html_attr<T>) {
            ctx.write(" ");
            ctx.write(T::key);
            ctx.write("=\"");
            ctx.write(T::value);
            ctx.write("\"");
        }
    }

    template <std::size_t... I>
    void render_children(render_ctx& ctx, std::index_sequence<I...>) const {
        (render_child_if<I>(ctx), ...);
    }

    template <std::size_t I>
    void render_child_if(render_ctx& ctx) const {
        using T = std::tuple_element_t<I, decltype(params.children)>;
        if constexpr (html_node<T>) {
            std::get<I>(params.children).render(ctx);
        }
    }
};

// Self-closing tag (void elements)
template <const char* Name, typename... Attrs>
struct void_tag : node {
    void render(render_ctx& ctx) const override {
        ctx.write("<");
        ctx.write(Name);

        // Write attributes
        (write_attr<Attrs>(ctx), ...);

        ctx.write(" />");
    }

private:
    template <typename A>
    static void write_attr(render_ctx& ctx) {
        ctx.write(" ");
        ctx.write(A::key);
        ctx.write("=\"");
        ctx.write(A::value);
        ctx.write("\"");
    }
};

// Tag creation helpers with compile-time string names
template <const char* Name, html_attr... A, html_node... C>
constexpr auto make_tag_impl(attrs<A...>, C&&... children) {
    return tag<Name, A..., std::decay_t<C>...>{
        A{}..., std::forward<C>(children)...
    };
}

template <const char* Name, html_node... C>
constexpr auto make_tag_impl(C&&... children) {
    return tag<Name, std::decay_t<C>...>{
        std::forward<C>(children)...
    };
}

}  // namespace protoflow::html
