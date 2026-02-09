#pragma once

#include "elements.hpp"
#include "text.hpp"
#include "attrs.hpp"

namespace protoflow::html {

// ============================================================================
// Status Badges
// ============================================================================

template <html_node... C>
constexpr auto badge_success(C&&... c) {
    return span(attrs<class_<"badge badge-success">>{}, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto badge_error(C&&... c) {
    return span(attrs<class_<"badge badge-error">>{}, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto badge_warning(C&&... c) {
    return span(attrs<class_<"badge badge-warning">>{}, std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto badge_info(C&&... c) {
    return span(attrs<class_<"badge badge-info">>{}, std::forward<C>(c)...);
}

// ============================================================================
// Cards
// ============================================================================

template <html_node Title, html_node... Content>
constexpr auto card(Title&& title, Content&&... content) {
    return div(
        attrs<class_<"card">>{},
        div(attrs<class_<"card-header">>{}, std::forward<Title>(title)),
        div(attrs<class_<"card-body">>{}, std::forward<Content>(content)...)
    );
}

template <html_node... Content>
constexpr auto card_simple(Content&&... content) {
    return div(
        attrs<class_<"card">>{},
        div(attrs<class_<"card-body">>{}, std::forward<Content>(content)...)
    );
}

// ============================================================================
// Data Display
// ============================================================================

template <html_node Label, html_node Value>
constexpr auto data_row(Label&& label, Value&& value) {
    return div(
        attrs<class_<"data-row">>{},
        span(attrs<class_<"data-label">>{}, std::forward<Label>(label)),
        span(attrs<class_<"data-value">>{}, std::forward<Value>(value))
    );
}

template <html_node... Cells>
constexpr auto table_row(Cells&&... cells) {
    return tr(attrs<class_<"table-row">>{}, std::forward<Cells>(cells)...);
}

template <html_node... Content>
constexpr auto table_header_cell(Content&&... content) {
    return th(attrs<class_<"table-header">>{}, std::forward<Content>(content)...);
}

template <html_node... Content>
constexpr auto table_data_cell(Content&&... content) {
    return td(attrs<class_<"table-cell">>{}, std::forward<Content>(content)...);
}

// ============================================================================
// Containers
// ============================================================================

template <html_node... Content>
constexpr auto container(Content&&... content) {
    return div(attrs<class_<"container">>{}, std::forward<Content>(content)...);
}

template <html_node... Content>
constexpr auto section_block(Content&&... content) {
    return div(attrs<class_<"section">>{}, std::forward<Content>(content)...);
}

template <html_node Title, html_node... Content>
constexpr auto section_with_title(Title&& title, Content&&... content) {
    return div(
        attrs<class_<"section">>{},
        div(attrs<class_<"section-title">>{}, std::forward<Title>(title)),
        div(attrs<class_<"section-content">>{}, std::forward<Content>(content)...)
    );
}

// ============================================================================
// Status Indicators
// ============================================================================

template <html_node... Content>
constexpr auto status_ok(Content&&... content) {
    return div(attrs<class_<"status status-ok">>{}, std::forward<Content>(content)...);
}

template <html_node... Content>
constexpr auto status_error(Content&&... content) {
    return div(attrs<class_<"status status-error">>{}, std::forward<Content>(content)...);
}

template <html_node... Content>
constexpr auto status_warning(Content&&... content) {
    return div(attrs<class_<"status status-warning">>{}, std::forward<Content>(content)...);
}

template <html_node... Content>
constexpr auto status_pending(Content&&... content) {
    return div(attrs<class_<"status status-pending">>{}, std::forward<Content>(content)...);
}

// ============================================================================
// Metric Display
// ============================================================================

template <html_node Label, html_node Value, html_node Unit>
constexpr auto metric(Label&& label, Value&& value, Unit&& unit) {
    return div(
        attrs<class_<"metric">>{},
        div(attrs<class_<"metric-label">>{}, std::forward<Label>(label)),
        div(
            attrs<class_<"metric-value-container">>{},
            span(attrs<class_<"metric-value">>{}, std::forward<Value>(value)),
            span(attrs<class_<"metric-unit">>{}, std::forward<Unit>(unit))
        )
    );
}

template <html_node Label, html_node Value>
constexpr auto metric_simple(Label&& label, Value&& value) {
    return div(
        attrs<class_<"metric">>{},
        div(attrs<class_<"metric-label">>{}, std::forward<Label>(label)),
        div(attrs<class_<"metric-value">>{}, std::forward<Value>(value))
    );
}

}  // namespace protoflow::html
