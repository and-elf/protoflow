#include <protoflow/html.hpp>
#include <gtest/gtest.h>

using namespace protoflow::html;

TEST(ElementsTest, DivElement) {
    auto element = div(text("Hello"));
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<div>Hello</div>");
}

TEST(ElementsTest, DivWithClass) {
    auto element = div(
        attrs<class_<"container">>{},
        text("Content")
    );
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<div class=\"container\">Content</div>");
}

TEST(ElementsTest, SpanElement) {
    auto element = span(text("Inline"));
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<span>Inline</span>");
}

TEST(ElementsTest, ParagraphElement) {
    auto element = p(text("Paragraph text"));
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<p>Paragraph text</p>");
}

TEST(ElementsTest, HeadingElements) {
    EXPECT_EQ(to_html(h1(text("H1"))), "<h1>H1</h1>");
    EXPECT_EQ(to_html(h2(text("H2"))), "<h2>H2</h2>");
    EXPECT_EQ(to_html(h3(text("H3"))), "<h3>H3</h3>");
    EXPECT_EQ(to_html(h4(text("H4"))), "<h4>H4</h4>");
    EXPECT_EQ(to_html(h5(text("H5"))), "<h5>H5</h5>");
    EXPECT_EQ(to_html(h6(text("H6"))), "<h6>H6</h6>");
}

TEST(ElementsTest, AnchorElement) {
    auto element = a(
        attrs<href_<"/home">>{},
        text("Home")
    );
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<a href=\"/home\">Home</a>");
}

TEST(ElementsTest, ListElements) {
    auto element = ul(
        li(text("Item 1")),
        li(text("Item 2")),
        li(text("Item 3"))
    );
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<ul><li>Item 1</li><li>Item 2</li><li>Item 3</li></ul>");
}

TEST(ElementsTest, OrderedList) {
    auto element = ol(
        attrs<class_<"numbered">>{},
        li(text("First")),
        li(text("Second"))
    );
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<ol class=\"numbered\"><li>First</li><li>Second</li></ol>");
}

TEST(ElementsTest, TableElements) {
    auto element = table(
        thead(
            tr(
                th(text("Name")),
                th(text("Value"))
            )
        ),
        tbody(
            tr(
                td(text("A")),
                td(text("1"))
            )
        )
    );
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<table><thead><tr><th>Name</th><th>Value</th></tr></thead><tbody><tr><td>A</td><td>1</td></tr></tbody></table>");
}

TEST(ElementsTest, VoidElements) {
    EXPECT_EQ(to_html(br()), "<br />");
    EXPECT_EQ(to_html(hr()), "<hr />");
}

TEST(ElementsTest, ImageElement) {
    auto element = img(attrs<src_<"photo.jpg">, alt_<"Photo">>{});
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<img src=\"photo.jpg\" alt=\"Photo\" />");
}

TEST(ElementsTest, ComplexNestedStructure) {
    auto element = div(
        attrs<class_<"card">>{},
        div(
            attrs<class_<"title">>{},
            h1(text("Title"))
        ),
        div(
            attrs<class_<"content">>{},
            p(text("Paragraph 1")),
            p(text("Paragraph 2"))
        ),
        div(
            attrs<class_<"status">>{},
            span(
                attrs<class_<"badge">>{},
                text("Status")
            )
        )
    );
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<div class=\"card\"><div class=\"title\"><h1>Title</h1></div><div class=\"content\"><p>Paragraph 1</p><p>Paragraph 2</p></div><div class=\"status\"><span class=\"badge\">Status</span></div></div>");
}

TEST(ElementsTest, ListWithLinks) {
    auto element = div(
        attrs<class_<"menu">>{},
        ul(
            li(a(attrs<href_<"/">>{}, text("Home"))),
            li(a(attrs<href_<"/about">>{}, text("About"))),
            li(a(attrs<href_<"/contact">>{}, text("Contact")))
        )
    );
    std::string result = to_html(element);
    
    EXPECT_EQ(result, "<div class=\"menu\"><ul><li><a href=\"/\">Home</a></li><li><a href=\"/about\">About</a></li><li><a href=\"/contact\">Contact</a></li></ul></div>");
}
