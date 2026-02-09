#include <protoflow/html.hpp>
#include <gtest/gtest.h>
#include <sstream>

using namespace protoflow::html;

TEST(FragmentTest, BasicFragment) {
    auto tag = div(text("Fragment content"));
    auto frag = make_fragment(std::move(tag));
    
    std::string result = frag->render();
    
    EXPECT_EQ(result, "<div>Fragment content</div>");
}

TEST(FragmentTest, ComplexFragment) {
    auto tag = div(
        attrs<class_<"container">>{},
        h1(text("Title")),
        p(text("Content"))
    );
    auto frag = make_fragment(std::move(tag));
    
    std::string result = frag->render();
    
    EXPECT_EQ(result, "<div class=\"container\"><h1>Title</h1><p>Content</p></div>");
}

TEST(RenderTest, ToHtmlFunction) {
    auto tag = div(
        attrs<id_<"main">>{},
        text("Main content")
    );
    
    std::string result = to_html(tag);
    
    EXPECT_EQ(result, "<div id=\"main\">Main content</div>");
}

TEST(RenderTest, RenderToStream) {
    auto tag = span(
        attrs<class_<"highlight">>{},
        text("Highlighted")
    );
    
    std::ostringstream oss;
    render(oss, tag);
    
    EXPECT_EQ(oss.str(), "<span class=\"highlight\">Highlighted</span>");
}

TEST(RenderTest, LargeDocument) {
    auto tag = div(
        attrs<class_<"dashboard">>{},
        div(
            attrs<class_<"header">>{},
            h1(text("Dashboard Title")),
            div(
                attrs<class_<"menu">>{},
                ul(
                    li(a(attrs<href_<"/">>{}, text("Home"))),
                    li(a(attrs<href_<"/about">>{}, text("About")))
                )
            )
        ),
        div(
            attrs<class_<"content">>{},
            div(
                attrs<class_<"widget">>{},
                h2(text("Widget Title")),
                p(text("First paragraph")),
                p(text("Second paragraph"))
            )
        ),
        div(
            attrs<class_<"footer">>{},
            p(text("Copyright 2026"))
        )
    );
    
    std::string result = to_html(tag);
    
    EXPECT_TRUE(result.find("<div class=\"dashboard\">") != std::string::npos);
    EXPECT_TRUE(result.find("<h1>Dashboard Title</h1>") != std::string::npos);
    EXPECT_TRUE(result.find("<div class=\"footer\">") != std::string::npos);
    EXPECT_TRUE(result.find("Copyright 2026") != std::string::npos);
}
