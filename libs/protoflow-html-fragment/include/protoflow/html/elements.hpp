#pragma once

#include "attrs.hpp"
#include "concepts.hpp"
#include "tag.hpp"
#include <utility>

namespace protoflow::html {

// Tag name constants
namespace tag_names {
    inline constexpr const char div_[] = "div";
    inline constexpr const char span_[] = "span";
    inline constexpr const char p_[] = "p";
    inline constexpr const char h1_[] = "h1";
    inline constexpr const char h2_[] = "h2";
    inline constexpr const char h3_[] = "h3";
    inline constexpr const char h4_[] = "h4";
    inline constexpr const char h5_[] = "h5";
    inline constexpr const char h6_[] = "h6";
    inline constexpr const char a_[] = "a";
    inline constexpr const char strong_[] = "strong";
    inline constexpr const char em_[] = "em";
    inline constexpr const char code_[] = "code";
    inline constexpr const char ul_[] = "ul";
    inline constexpr const char ol_[] = "ol";
    inline constexpr const char li_[] = "li";
    inline constexpr const char table_[] = "table";
    inline constexpr const char thead_[] = "thead";
    inline constexpr const char tbody_[] = "tbody";
    inline constexpr const char tr_[] = "tr";
    inline constexpr const char th_[] = "th";
    inline constexpr const char td_[] = "td";
    inline constexpr const char br_[] = "br";
    inline constexpr const char hr_[] = "hr";
    inline constexpr const char img_[] = "img";
}

// ============================================================================
// Basic structure elements
// ============================================================================

template <html_node... C>
constexpr auto div(C&&... c) {
    return make_tag_impl<tag_names::div_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto div(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::div_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto span(C&&... c) {
    return make_tag_impl<tag_names::span_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto span(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::span_>(a, std::forward<C>(c)...);
}

// ============================================================================
// Text content
// ============================================================================

template <html_node... C>
constexpr auto p(C&&... c) {
    return make_tag_impl<tag_names::p_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto p(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::p_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto h1(C&&... c) {
    return make_tag_impl<tag_names::h1_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto h1(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::h1_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto h2(C&&... c) {
    return make_tag_impl<tag_names::h2_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto h2(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::h2_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto h3(C&&... c) {
    return make_tag_impl<tag_names::h3_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto h3(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::h3_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto h4(C&&... c) {
    return make_tag_impl<tag_names::h4_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto h4(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::h4_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto h5(C&&... c) {
    return make_tag_impl<tag_names::h5_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto h5(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::h5_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto h6(C&&... c) {
    return make_tag_impl<tag_names::h6_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto h6(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::h6_>(a, std::forward<C>(c)...);
}

// ============================================================================
// Links and inline formatting
// ============================================================================

template <html_node... C>
constexpr auto a(C&&... c) {
    return make_tag_impl<tag_names::a_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto a(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::a_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto strong(C&&... c) {
    return make_tag_impl<tag_names::strong_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto strong(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::strong_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto em(C&&... c) {
    return make_tag_impl<tag_names::em_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto em(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::em_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto code(C&&... c) {
    return make_tag_impl<tag_names::code_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto code(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::code_>(a, std::forward<C>(c)...);
}

// ============================================================================
// Lists
// ============================================================================

template <html_node... C>
constexpr auto ul(C&&... c) {
    return make_tag_impl<tag_names::ul_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto ul(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::ul_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto ol(C&&... c) {
    return make_tag_impl<tag_names::ol_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto ol(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::ol_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto li(C&&... c) {
    return make_tag_impl<tag_names::li_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto li(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::li_>(a, std::forward<C>(c)...);
}

// ============================================================================
// Tables
// ============================================================================

template <html_node... C>
constexpr auto table(C&&... c) {
    return make_tag_impl<tag_names::table_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto table(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::table_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto thead(C&&... c) {
    return make_tag_impl<tag_names::thead_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto thead(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::thead_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto tbody(C&&... c) {
    return make_tag_impl<tag_names::tbody_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto tbody(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::tbody_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto tr(C&&... c) {
    return make_tag_impl<tag_names::tr_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto tr(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::tr_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto th(C&&... c) {
    return make_tag_impl<tag_names::th_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto th(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::th_>(a, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto td(C&&... c) {
    return make_tag_impl<tag_names::td_>(std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto td(attrs<A...> a, C&&... c) {
    return make_tag_impl<tag_names::td_>(a, std::forward<C>(c)...);
}

// ============================================================================
// Void elements (self-closing)
// ============================================================================

template <html_attr... A>
constexpr auto br(attrs<A...>) {
    return void_tag<tag_names::br_, A...>{};
}

inline auto br() {
    return void_tag<tag_names::br_>{};
}

template <html_attr... A>
constexpr auto hr(attrs<A...>) {
    return void_tag<tag_names::hr_, A...>{};
}

inline auto hr() {
    return void_tag<tag_names::hr_>{};
}

template <html_attr... A>
constexpr auto img(attrs<A...>) {
    return void_tag<tag_names::img_, A...>{};
}


}  // namespace protoflow::html
