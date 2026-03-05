#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>

using namespace beast;

// ── Helpers ───────────────────────────────────────────────────────────────────

static Value parse_root(Document &doc, std::string_view json) {
  return parse(doc, json);
}

// ── Type checkers ─────────────────────────────────────────────────────────────

TEST(ValueAccessors, TypeCheckers) {
  Document doc;
  {
    auto v = parse_root(doc, "null");
    EXPECT_TRUE(v.is_null());
    EXPECT_FALSE(v.is_bool());
    EXPECT_FALSE(v.is_int());
    EXPECT_FALSE(v.is_string());
  }
  {
    auto v = parse_root(doc, "true");
    EXPECT_TRUE(v.is_bool());
    EXPECT_FALSE(v.is_null());
  }
  {
    auto v = parse_root(doc, "false");
    EXPECT_TRUE(v.is_bool());
  }
  {
    auto v = parse_root(doc, "42");
    EXPECT_TRUE(v.is_int());
    EXPECT_TRUE(v.is_number());
    EXPECT_FALSE(v.is_double());
  }
  {
    auto v = parse_root(doc, "3.14");
    EXPECT_TRUE(v.is_number());
    EXPECT_FALSE(v.is_int());
  }
  {
    auto v = parse_root(doc, "\"hello\"");
    EXPECT_TRUE(v.is_string());
    EXPECT_FALSE(v.is_int());
  }
  {
    auto v = parse_root(doc, "{}");
    EXPECT_TRUE(v.is_object());
    EXPECT_FALSE(v.is_array());
  }
  {
    auto v = parse_root(doc, "[]");
    EXPECT_TRUE(v.is_array());
    EXPECT_FALSE(v.is_object());
  }
}

// ── as<T>(): typed extraction ─────────────────────────────────────────────────

TEST(ValueAccessors, AsInt) {
  Document doc;
  auto v = parse_root(doc, "42");
  EXPECT_EQ(v.as<int64_t>(), 42);
  EXPECT_EQ(v.as<int>(), 42);
  EXPECT_EQ(v.as<unsigned>(), 42u);
}

TEST(ValueAccessors, AsNegativeInt) {
  Document doc;
  auto v = parse_root(doc, "-7");
  EXPECT_EQ(v.as<int64_t>(), -7);
  EXPECT_EQ(v.as<int>(), -7);
}

TEST(ValueAccessors, AsDouble) {
  Document doc;
  auto v = parse_root(doc, "3.14");
  EXPECT_NEAR(v.as<double>(), 3.14, 1e-9);
}

TEST(ValueAccessors, AsDoubleFromInt) {
  Document doc;
  auto v = parse_root(doc, "10");
  EXPECT_NEAR(v.as<double>(), 10.0, 1e-9);
}

TEST(ValueAccessors, AsBool) {
  Document doc;
  EXPECT_TRUE(parse_root(doc, "true").as<bool>());
  EXPECT_FALSE(parse_root(doc, "false").as<bool>());
}

TEST(ValueAccessors, AsStringView) {
  Document doc;
  auto v = parse_root(doc, "\"beast\"");
  EXPECT_EQ(v.as<std::string_view>(), "beast");
}

TEST(ValueAccessors, AsString) {
  Document doc;
  auto v = parse_root(doc, "\"hello world\"");
  EXPECT_EQ(v.as<std::string>(), "hello world");
}

TEST(ValueAccessors, AsTypeMismatchThrows) {
  Document doc;
  auto v = parse_root(doc, "\"not a number\"");
  EXPECT_THROW(v.as<int64_t>(), std::runtime_error);
  EXPECT_THROW(v.as<bool>(), std::runtime_error);
}

// ── try_as<T>(): safe optional extraction ─────────────────────────────────────

TEST(ValueAccessors, TryAsSuccess) {
  Document doc;
  auto v = parse_root(doc, "99");
  auto result = v.try_as<int64_t>();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 99);
}

TEST(ValueAccessors, TryAsFailure) {
  Document doc;
  auto v = parse_root(doc, "\"text\"");
  EXPECT_FALSE(v.try_as<int64_t>().has_value());
  EXPECT_FALSE(v.try_as<double>().has_value());
  EXPECT_FALSE(v.try_as<bool>().has_value());
}

TEST(ValueAccessors, TryAsBool) {
  Document doc;
  EXPECT_EQ(parse_root(doc, "true").try_as<bool>(), std::optional<bool>(true));
  EXPECT_EQ(parse_root(doc, "false").try_as<bool>(), std::optional<bool>(false));
  EXPECT_FALSE(parse_root(doc, "0").try_as<bool>().has_value());
}

// ── operator[](key): object access ───────────────────────────────────────────

TEST(ValueAccessors, ObjectSubscript) {
  Document doc;
  auto root = parse_root(doc, R"({"age": 30, "name": "Alice"})");
  EXPECT_EQ(root["age"].as<int>(), 30);
  EXPECT_EQ(root["name"].as<std::string>(), "Alice");
}

TEST(ValueAccessors, ObjectSubscriptMissingKeyThrows) {
  Document doc;
  auto root = parse_root(doc, R"({"x": 1})");
  EXPECT_THROW(root["missing"], std::out_of_range);
}

TEST(ValueAccessors, ObjectSubscriptOnNonObjectThrows) {
  Document doc;
  auto root = parse_root(doc, "[1, 2]");
  EXPECT_THROW(root["key"], std::runtime_error);
}

// ── operator[](idx): array access ─────────────────────────────────────────────

TEST(ValueAccessors, ArraySubscript) {
  Document doc;
  auto root = parse_root(doc, "[10, 20, 30]");
  EXPECT_EQ(root[0].as<int>(), 10);
  EXPECT_EQ(root[1].as<int>(), 20);
  EXPECT_EQ(root[2].as<int>(), 30);
}

TEST(ValueAccessors, ArraySubscriptOutOfRangeThrows) {
  Document doc;
  auto root = parse_root(doc, "[1, 2]");
  EXPECT_THROW(root[5], std::out_of_range);
}

TEST(ValueAccessors, ArraySubscriptOnNonArrayThrows) {
  Document doc;
  auto root = parse_root(doc, R"({"a": 1})");
  EXPECT_THROW(root[0], std::runtime_error);
}

// ── find(): safe object lookup ────────────────────────────────────────────────

TEST(ValueAccessors, FindPresent) {
  Document doc;
  auto root = parse_root(doc, R"({"score": 100})");
  auto result = root.find("score");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->as<int>(), 100);
}

TEST(ValueAccessors, FindAbsent) {
  Document doc;
  auto root = parse_root(doc, R"({"a": 1})");
  EXPECT_FALSE(root.find("missing").has_value());
}

TEST(ValueAccessors, FindOnNonObject) {
  Document doc;
  auto root = parse_root(doc, "[1]");
  EXPECT_FALSE(root.find("key").has_value());
}

// ── size() and empty() ────────────────────────────────────────────────────────

TEST(ValueAccessors, SizeArray) {
  Document doc;
  EXPECT_EQ(parse_root(doc, "[]").size(), 0u);
  EXPECT_EQ(parse_root(doc, "[1]").size(), 1u);
  EXPECT_EQ(parse_root(doc, "[1,2,3]").size(), 3u);
}

TEST(ValueAccessors, SizeObject) {
  Document doc;
  EXPECT_EQ(parse_root(doc, "{}").size(), 0u);
  EXPECT_EQ(parse_root(doc, R"({"a":1})").size(), 1u);
  EXPECT_EQ(parse_root(doc, R"({"a":1,"b":2})").size(), 2u);
}

TEST(ValueAccessors, EmptyCheck) {
  Document doc;
  EXPECT_TRUE(parse_root(doc, "[]").empty());
  EXPECT_FALSE(parse_root(doc, "[1]").empty());
  EXPECT_TRUE(parse_root(doc, "{}").empty());
}

// ── Implicit conversion (operator T()) ────────────────────────────────────────

TEST(ValueAccessors, ImplicitInt) {
  Document doc;
  auto root = parse_root(doc, R"({"age": 25})");
  int age = root["age"]; // implicit conversion
  EXPECT_EQ(age, 25);
}

TEST(ValueAccessors, ImplicitString) {
  Document doc;
  auto root = parse_root(doc, R"({"name": "Beast"})");
  std::string name = root["name"]; // implicit conversion
  EXPECT_EQ(name, "Beast");
}

TEST(ValueAccessors, ImplicitDouble) {
  Document doc;
  auto root = parse_root(doc, R"({"ratio": 1.5})");
  double ratio = root["ratio"];
  EXPECT_NEAR(ratio, 1.5, 1e-9);
}

TEST(ValueAccessors, ImplicitBool) {
  Document doc;
  auto root = parse_root(doc, R"({"flag": true})");
  bool flag = root["flag"];
  EXPECT_TRUE(flag);
}

// ── Chained access ────────────────────────────────────────────────────────────

TEST(ValueAccessors, ChainedObjectAccess) {
  Document doc;
  auto root = parse_root(doc, R"({"user": {"id": 7, "name": "Eve"}})");
  EXPECT_EQ(root["user"]["id"].as<int>(), 7);
  EXPECT_EQ(root["user"]["name"].as<std::string>(), "Eve");
}

TEST(ValueAccessors, ChainedArrayAccess) {
  Document doc;
  auto root = parse_root(doc, R"({"ids": [10, 20, 30]})");
  EXPECT_EQ(root["ids"][0].as<int>(), 10);
  EXPECT_EQ(root["ids"][2].as<int>(), 30);
}

TEST(ValueAccessors, DeepNested) {
  Document doc;
  auto root =
      parse_root(doc, R"({"a": {"b": {"c": 42}}})");
  EXPECT_EQ(root["a"]["b"]["c"].as<int>(), 42);
}

// ── Mixed-type arrays ─────────────────────────────────────────────────────────

TEST(ValueAccessors, MixedArray) {
  Document doc;
  auto root = parse_root(doc, R"([1, "two", true, null])");
  EXPECT_EQ(root[0].as<int>(), 1);
  EXPECT_EQ(root[1].as<std::string>(), "two");
  EXPECT_TRUE(root[2].as<bool>());
  EXPECT_TRUE(root[3].is_null());
}

// ── Boolean validity check (operator bool()) ──────────────────────────────────

TEST(ValueAccessors, ValidityCheck) {
  Document doc;
  auto root = parse_root(doc, "42");
  EXPECT_TRUE(static_cast<bool>(root));
  Value empty;
  EXPECT_FALSE(static_cast<bool>(empty));
}
