#include <protoflow/html.hpp>
#include <gtest/gtest.h>

using namespace protoflow::html;

TEST(TextTest, SimpleText) {
    text t("Hello World");
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "Hello World");
}

TEST(TextTest, HTMLEscaping) {
    text t("<div>Test & \"quoted\" 'text'</div>");
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "&lt;div&gt;Test &amp; &quot;quoted&quot; &#39;text&#39;&lt;/div&gt;");
}

TEST(TextTest, EmptyText) {
    text t("");
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "");
}

TEST(TextTest, EscapeLessThan) {
    text t("5 < 10");
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "5 &lt; 10");
}

TEST(TextTest, EscapeGreaterThan) {
    text t("10 > 5");
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "10 &gt; 5");
}

TEST(TextTest, EscapeAmpersand) {
    text t("AT&T");
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "AT&amp;T");
}

TEST(TextTest, EscapeQuotes) {
    text t("\"Hello\" and 'World'");
    render_ctx ctx;
    
    t.render(ctx);
    
    EXPECT_EQ(ctx.result(), "&quot;Hello&quot; and &#39;World&#39;");
}
