#include <protoflow/html.hpp>
#include <gtest/gtest.h>

using namespace protoflow::html;

TEST(TagTest, SimpleTag) {
    auto t = div(text("Hello"));
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "<div>Hello</div>");
}

TEST(TagTest, NestedTags) {
    auto t = div(
        p(text("Paragraph"))
    );
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "<div><p>Paragraph</p></div>");
}

TEST(TagTest, TagWithAttributes) {
    auto t = div(
        attrs<class_<"container">>{},
        text("Content")
    );
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "<div class=\"container\">Content</div>");
}

TEST(TagTest, TagWithMultipleAttributes) {
    auto t = div(
        attrs<class_<"container">, id_<"main">>{},
        text("Content")
    );
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "<div class=\"container\" id=\"main\">Content</div>");
}

TEST(TagTest, TagWithMultipleChildren) {
    auto t = div(
        text("First"),
        text("Second"),
        text("Third")
    );
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "<div>FirstSecondThird</div>");
}

TEST(TagTest, ComplexNesting) {
    auto t = div(
        attrs<class_<"outer">>{},
        div(
            attrs<class_<"inner">>{},
            text("Content")
        )
    );
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "<div class=\"outer\"><div class=\"inner\">Content</div></div>");
}

TEST(VoidTagTest, SimpleVoidTag) {
    auto t = br();
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "<br />");
}

TEST(VoidTagTest, VoidTagWithAttributes) {
    auto t = img(attrs<src_<"image.jpg">, alt_<"An image">>{});
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "<img src=\"image.jpg\" alt=\"An image\" />");
}
