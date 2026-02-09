#include <protoflow/html.hpp>
#include <gtest/gtest.h>
#include <string_view>

using namespace protoflow::html;

TEST(FixedStringTest, BasicConstruction) {
    constexpr fixed_string<5> str{"hello"};
    
    EXPECT_EQ(str.size(), 5);  // String length without null terminator
    EXPECT_STREQ(str.data, "hello");
}

TEST(FixedStringTest, StringViewConversion) {
    constexpr fixed_string<5> str{"world"};
    std::string_view sv = str.view();
    
    EXPECT_EQ(sv, "world");
    EXPECT_EQ(sv.size(), 5);  // String view excludes null terminator
}

TEST(FixedStringTest, EmptyString) {
    constexpr fixed_string<0> str{""};
    
    EXPECT_EQ(str.size(), 0);  // Empty string has size 0
    EXPECT_STREQ(str.data, "");
}

TEST(FixedStringTest, LongerString) {
    constexpr fixed_string<23> str{"This is a longer string"};
    std::string_view sv = str.view();
    
    EXPECT_EQ(sv, "This is a longer string");
}
