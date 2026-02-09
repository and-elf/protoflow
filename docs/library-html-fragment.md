# protoflow-html-fragment

Compile-time HTML generation library using C++23 concepts.

## Features

- Type-safe HTML construction
- Constexpr tag building
- Fragment composition
- Compile-time validation
- Zero runtime overhead

---

## Concepts

### tag
```cpp
template <fixed_string Name, typename... Attrs, typename... Children>
struct tag : node {
    std::tuple<Children...> children;

    constexpr tag(Children... c) : children(std::move(c)...) {}

    void render(render_ctx& ctx) const override {
        ctx.write("<");
        ctx.write(Name);

        (write_attr<Attrs>(ctx), ...);

        ctx.write(">");

        std::apply([&](auto const&... c) {
            (c.render(ctx), ...);
        }, children);

        ctx.write("</");
        ctx.write(Name);
        ctx.write(">");
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
```

### html_node

```cpp
template <typename T>
concept html_node = std::derived_from<T, html::node>;
```

Represents any HTML element or text node.

### html_attr

```cpp
template <typename T>
concept html_attr =
    requires {
        { T::key }   -> std::convertible_to<std::string_view>;
        { T::value } -> std::convertible_to<std::string_view>;
    };
```

Represents HTML attributes (e.g., `class`, `id`, `style`).

### html_string

```cpp
template <typename T>
concept html_string =
    requires {
        { T::size } -> std::convertible_to<std::size_t>;
        requires std::same_as<decltype(T::value), char[T::size]>;
    };
```

Compile-time string representation for tag names.

---

## Tag Construction

### Generic Tag Creation

```cpp
template <html_string Name, html_attr... A, html_node... C>
constexpr auto make_tag(attrs<A...>, C&&... children) {
    return tag<Name, A..., std::decay_t<C>...>{
        std::forward<C>(children)...
    };
}

template <html_string Name, html_node... C>
constexpr auto make_tag(C&&... children) {
    return tag<Name, std::decay_t<C>...>{
        std::forward<C>(children)...
    };
}
```

---

## Element Helpers

### Basic Elements (No Attributes)

```cpp
template <html_node... C>
constexpr auto div(C&&... c) {
    return make_tag<fixed_string{"div"}>(std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto span(C&&... c) {
    return make_tag<fixed_string{"span"}>(std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto p(C&&... c) {
    return make_tag<fixed_string{"p"}>(std::forward<C>(c)...);
}

template <html_node... C>
constexpr auto h1(C&&... c) {
    return make_tag<fixed_string{"h1"}>(std::forward<C>(c)...);
}
```

### Elements With Attributes

```cpp
template <html_attr... A, html_node... C>
constexpr auto div(attrs<A...> a, C&&... c) {
    return make_tag<fixed_string{"div"}>(a, std::forward<C>(c)...);
}

template <html_attr... A, html_node... C>
constexpr auto span(attrs<A...> a, C&&... c) {
    return make_tag<fixed_string{"span"}>(a, std::forward<C>(c)...);
}
```

---

## Usage Examples

### Simple Tags

```cpp
// <div>Hello World</div>
auto greeting = div(text("Hello World"));

// <h1>Title</h1>
// <p>Paragraph</p>
auto content = div(
    h1(text("Title")),
    p(text("Paragraph"))
);
```

### With Attributes

```cpp
// <div class="container">
//   <span id="main">Content</span>
// </div>
auto styled = div(
    attrs<class_{"container"}>{},
    span(
        attrs<id_{"main"}>{},
        text("Content")
    )
);
```

### Nested Structures

```cpp
// <div class="card">
//   <h1>Dashboard</h1>
//   <div class="content">
//     <p>System Status: Online</p>
//     <span class="badge">Active</span>
//   </div>
// </div>
auto card = div(
    attrs<class_{"card"}>{},
    h1(text("Dashboard")),
    div(
        attrs<class_{"content"}>{},
        p(text("System Status: Online")),
        span(
            attrs<class_{"badge"}>{},
            text("Active")
        )
    )
);
```

### Fragment Composition

```cpp
auto nav_item(std::string_view label, std::string_view href) {
    return li(
        a(attrs<href_{href}>{}, text(label))
    );
}

auto navbar = ul(
    attrs<class_{"nav"}>{},
    nav_item("Home", "/"),
    nav_item("About", "/about"),
    nav_item("Contact", "/contact")
);
```

---

## Attribute Definitions

### Common Attributes

```cpp
// Class attribute
template <fixed_string Value>
struct class_ {
    static constexpr auto key = "class";
    static constexpr auto value = Value.value;
};

// ID attribute
template <fixed_string Value>
struct id_ {
    static constexpr auto key = "id";
    static constexpr auto value = Value.value;
};

// Href attribute
template <fixed_string Value>
struct href_ {
    static constexpr auto key = "href";
    static constexpr auto value = Value.value;
};

// Style attribute
template <fixed_string Value>
struct style_ {
    static constexpr auto key = "style";
    static constexpr auto value = Value.value;
};
```

### Attribute Pack

```cpp
template <fixed_string Key, fixed_string Value>
struct attr {
    static constexpr std::string_view key = Key;
    static constexpr std::string_view value = Value;
};
```

---

## Rendering

### To String

```cpp
template <typename Tag>
std::string to_html(const Tag& tag) {
    std::string result;
    tag.render(result);
    return result;
}
```

### To Stream

```cpp
template <typename Tag>
void render(std::ostream& os, const Tag& tag) {
    std::string html = to_html(tag);
    os << html;
}
```

---

## Fragment Interface

```cpp
class fragment {
public:
    virtual ~fragment() = default;
    virtual std::string render() const = 0;
};

template <typename Tag>
class fragment_impl : public fragment {
    Tag tag_;
public:
    explicit fragment_impl(Tag tag) : tag_(std::move(tag)) {}
    
    std::string render() const override {
        return to_html(tag_);
    }
};
```

---

## Advanced Example: Dashboard Fragment

```cpp
html::fragment render_dashboard(const AppStatus& status) {
    return div(
        attrs<class_{"dashboard"}>{},
        
        // Header
        div(
            attrs<class_{"header"}>{},
            h1(text("Application Dashboard")),
            span(
                attrs<class_{"status-badge"}>{},
                text(status.is_alive ? "Online" : "Offline")
            )
        ),
        
        // Stats
        div(
            attrs<class_{"stats"}>{},
            div(
                attrs<class_{"stat"}>{},
                span(attrs<class_{"label"}>{}, text("Uptime:")),
                span(attrs<class_{"value"}>{}, text(status.uptime))
            ),
            div(
                attrs<class_{"stat"}>{},
                span(attrs<class_{"label"}>{}, text("Messages:")),
                span(attrs<class_{"value"}>{}, text(std::to_string(status.msg_count)))
            )
        )
    );
}
```

---

## Benefits

✓ **Type-safe** HTML construction  
✓ **Compile-time validation** of structure  
✓ **Zero runtime overhead** for tag creation  
✓ **Composable** fragments  
✓ **IDE autocomplete** support  
✓ **No string concatenation** vulnerabilities  
✓ **Constexpr** where possible  

---

## Implementation Notes

- Uses C++23 concepts for type safety
- `fixed_string` for compile-time string handling
- Template metaprogramming for tag composition
- No dynamic allocation during construction
- Rendering happens at call site (controlled by user)
- Compatible with embedded systems (minimal overhead)
