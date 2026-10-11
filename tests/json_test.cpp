#include "json.h"

#include <gtest/gtest.h>

using launcher::JsonValue;
using launcher::parse_json;

TEST(Json, ValuesOfEveryKind) {
    const auto value = parse_json(R"({"a": [1, -2.5e1, true, false, null, "x"], "b": {"c": "d"}, "e": ""})");
    ASSERT_TRUE(value.has_value()) << value.error();
    ASSERT_TRUE(value->is(JsonValue::Kind::Object));
    const JsonValue* a = value->find("a");
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->array.size(), 6u);
    EXPECT_DOUBLE_EQ(a->array[0].number, 1.0);
    EXPECT_DOUBLE_EQ(a->array[1].number, -25.0);
    EXPECT_TRUE(a->array[2].boolean);
    EXPECT_FALSE(a->array[3].boolean);
    EXPECT_TRUE(a->array[4].is(JsonValue::Kind::Null));
    EXPECT_EQ(a->array[5].string, "x");
    EXPECT_EQ(value->find("b")->find("c")->string, "d");
    EXPECT_EQ(value->find("e")->string, "");
    EXPECT_EQ(value->find("missing"), nullptr);
    EXPECT_EQ(a->find("x"), nullptr) << "find is for objects";
}

TEST(Json, StringEscapes) {
    const auto value = parse_json(R"("a\"b\\c\/d\n\t\u00e9\u20ac\ud83d\ude00")");
    ASSERT_TRUE(value.has_value()) << value.error();
    EXPECT_EQ(value->string, "a\"b\\c/d\n\t\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80");
}

TEST(Json, SurroundingWhiteSpace) {
    EXPECT_TRUE(parse_json(" \r\n\t[ ] \n").has_value());
    EXPECT_TRUE(parse_json("{}").has_value());
}

TEST(Json, Errors) {
    for (const char* text : {"", "   ", "[", "{", "[1,]", "{\"a\"}", "{\"a\":1,}", "[1 2]", "tru", "nul", "01", "1.", "-",
                 "1e", "\"abc", "\"\\x\"", "\"\\u12\"", "\"\\ud83d\"", "\"\\ude00\"", "[1] x", "{'a':1}", "\"\x01\"",
                 "1e999"}) {
        EXPECT_FALSE(parse_json(text).has_value()) << text;
    }
}

TEST(Json, TheErrorNamesTheOffset) {
    const auto value = parse_json("[1, 2, x]");
    ASSERT_FALSE(value.has_value());
    EXPECT_NE(value.error().find("byte 7"), std::string::npos) << value.error();
}

TEST(Json, DeepNestingIsRefusedNotCrashed) {
    EXPECT_FALSE(parse_json(std::string(100000, '[')).has_value());
    EXPECT_TRUE(parse_json(std::string(60, '[') + std::string(60, ']')).has_value());
    EXPECT_FALSE(parse_json(std::string(70, '[') + std::string(70, ']')).has_value());
}

TEST(Json, TheFirstMemberOfARepeatedNameWins) {
    const auto value = parse_json(R"({"a": 1, "a": 2})");
    ASSERT_TRUE(value.has_value());
    EXPECT_DOUBLE_EQ(value->find("a")->number, 1.0);
}
