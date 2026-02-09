#include <protoflow/http/json.hpp>
#include <gtest/gtest.h>
#include <vector>
#include <optional>

using namespace protoflow::http::json;

struct User {
    std::string name;
    int age;
    bool active;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(User, name, age, active)

TEST(JsonTest, BasicTypes) {
    EXPECT_EQ(to_json(true), "true");
    EXPECT_EQ(to_json(false), "false");
    EXPECT_EQ(to_json(42), "42");
    EXPECT_EQ(to_json(3.14), "3.14");
    EXPECT_EQ(to_json("hello"), "\"hello\"");
}

TEST(JsonTest, StringEscaping) {
    EXPECT_EQ(to_json("hello\"world"), "\"hello\\\"world\"");
    EXPECT_EQ(to_json("line1\nline2"), "\"line1\\nline2\"");
    EXPECT_EQ(to_json("tab\there"), "\"tab\\there\"");
    EXPECT_EQ(to_json("back\\slash"), "\"back\\\\slash\"");
}

TEST(JsonTest, OptionalTypes) {
    std::optional<int> some_value = 42;
    std::optional<int> no_value;
    
    EXPECT_EQ(to_json(some_value), "42");
    EXPECT_EQ(to_json(no_value), "null");
}

TEST(JsonTest, VectorTypes) {
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    EXPECT_EQ(to_json(numbers), "[1,2,3,4,5]");
    
    std::vector<std::string> strings = {"one", "two", "three"};
    EXPECT_EQ(to_json(strings), "[\"one\",\"two\",\"three\"]");
    
    std::vector<int> empty;
    EXPECT_EQ(to_json(empty), "[]");
}

TEST(JsonTest, MacroFieldNames) {
    User user{"Alice", 30, true};
    std::string json = to_json(user);
    
    // Check that all fields are present
    EXPECT_NE(json.find("\"name\""), std::string::npos);
    EXPECT_NE(json.find("\"Alice\""), std::string::npos);
    EXPECT_NE(json.find("\"age\""), std::string::npos);
    EXPECT_NE(json.find("30"), std::string::npos);
    EXPECT_NE(json.find("\"active\""), std::string::npos);
    EXPECT_NE(json.find("true"), std::string::npos);
}
