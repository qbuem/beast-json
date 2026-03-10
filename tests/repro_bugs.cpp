#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <set>
#include <string>

using namespace beast;

// Bug 1: Segfault after moving Document
TEST(ReproBugs, MoveDocumentSegfault) {
  Document doc;
  Value root = parse(doc, R"({"key": 42})");
  ASSERT_TRUE(root.is_object());

  Document doc2 = std::move(doc);
  // root still points to &doc, but doc is moved (tape.base is null)
  // Actually, root.doc_ is &doc. Let's see if it segfaults.
  // In many cases, it will if the move didn't change the address but cleared
  // internals.
  // root still points to the same DocumentState which is now shared.
  EXPECT_EQ(root["key"].as<int>(), 42);
}

// Bug 2: insert() is ignored by operator[]
TEST(ReproBugs, InsertIgnoredBySubscript) {
  Document doc;
  Value root = parse(doc, "{}");
  root.insert("new_key", 100);

  // This currently fails in the existing implementation
  Value v = root["new_key"];
  EXPECT_TRUE(v.is_valid());
  if (v.is_valid()) {
    EXPECT_EQ(v.as<int>(), 100);
  }
}

// Bug 3: Array size() ignores array_insertions_
TEST(ReproBugs, ArraySizeIgnoresInsertions) {
  Document doc;
  Value root = parse(doc, "[1, 2]");
  root.insert_json(1, "10"); // Insert 10 at index 1 -> [1, 10, 2]

  // This currently returns 2, should be 3
  EXPECT_EQ(root.size(), 3u);
}

// Bug 4: erase() is ignored by as<T>()
TEST(ReproBugs, EraseIgnoredByAs) {
  Document doc;
  Value root = parse(doc, R"({"key": 42})");
  Value v = root["key"];
  root.erase("key");

  EXPECT_FALSE(root.contains("key"));
  // v still points to tape index, so as<int>() might still return 42
  // but the key is logically erased.
  EXPECT_THROW(v.as<int>(), std::runtime_error);
}

// ── BUG-1: beast::parse_reuse not exposed in public facade ───────────────────
//
// beast::json::parse_reuse existed internally but beast::parse_reuse was not
// forwarded in the public beast:: namespace.
// Use explicit qualification to avoid ADL ambiguity with beast::json::parse_reuse.
TEST(ReproBugs2, ParseReusePublicFacade) {
  beast::Document doc;
  // First parse
  beast::Value v1 = beast::parse_reuse(doc, R"({"a":1})");
  EXPECT_TRUE(v1.is_object());
  EXPECT_EQ(v1["a"].as<int>(), 1);

  // Reuse same document handle for a second parse
  beast::Value v2 = beast::parse_reuse(doc, R"({"b":2})");
  EXPECT_TRUE(v2.is_object());
  EXPECT_EQ(v2["b"].as<int>(), 2);
}

// ── BUG-2: unsigned int overload ambiguity ───────────────────────────────────
//
// unsigned int literals (1u, 0u) were ambiguous between size_t and int
// overloads for Value::operator[], Value::erase, SafeValue::get.
TEST(ReproBugs2, UnsignedIntSubscriptNoAmbiguity) {
  Document doc;
  Value arr = parse(doc, "[10,20,30]");

  // All of these must compile and return correct values without a cast
  EXPECT_EQ(arr[0u].as<int>(), 10);
  EXPECT_EQ(arr[1u].as<int>(), 20);
  EXPECT_EQ(arr[2u].as<int>(), 30);
}

TEST(ReproBugs2, UnsignedIntEraseNoAmbiguity) {
  Document doc;
  Value arr = parse(doc, "[10,20,30]");

  // erase(unsigned int) must compile and erase the correct element
  arr.erase(1u);
  EXPECT_EQ(arr.dump(), "[10,30]");
  EXPECT_EQ(arr.size(), 2u);
}

TEST(ReproBugs2, SafeValueGetUnsignedIntNoAmbiguity) {
  Document doc;
  Value arr = parse(doc, "[10,20,30]");
  SafeValue sv(arr);

  // SafeValue::get(unsigned int) must compile without cast
  auto v0 = sv.get(0u);
  ASSERT_TRUE(v0.has_value());
  EXPECT_EQ(v0.as<int>().value_or(-1), 10);

  auto v1 = sv[1u];
  ASSERT_TRUE(v1.has_value());
  EXPECT_EQ(v1.as<int>().value_or(-1), 20);
}

// ── BUG-3: push_back() / push_back_json() leaves size() stale ────────────────
//
// After push_back(), size() returned the original parsed count (frozen)
// because additions_ was not counted in the array size calculation.
TEST(ReproBugs2, PushBackSizeReflectsNewElements) {
  Document doc;
  Value v = parse(doc, R"([])");

  EXPECT_EQ(v.size(), 0u);
  v.push_back(1);
  EXPECT_EQ(v.size(), 1u);
  v.push_back(2.5);
  EXPECT_EQ(v.size(), 2u);
  v.push_back("hello");
  EXPECT_EQ(v.size(), 3u);

  EXPECT_EQ(v.dump(), R"([1,2.5,"hello"])");
}

TEST(ReproBugs2, PushBackOnNonEmptyArraySizeCorrect) {
  Document doc;
  Value v = parse(doc, R"([10, 20])");

  ASSERT_EQ(v.size(), 2u);
  v.push_back(30);
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v.dump(), "[10,20,30]");
}

TEST(ReproBugs2, PushBackJsonSizeReflectsNewElements) {
  Document doc;
  Value v = parse(doc, "[1,2,3]");

  v.push_back_json("4");
  EXPECT_EQ(v.size(), 4u);
  EXPECT_EQ(v.dump(), "[1,2,3,4]");
}

// ── BUG-4: insert() leaves items() iteration truncated ───────────────────────
//
// After insert(), items() only iterated original parsed keys.
// Newly inserted keys were invisible in range-based for loop.
TEST(ReproBugs2, ItemsIncludesInsertedKeys) {
  Document doc;
  Value v = parse(doc, R"({"a":1,"b":2})");
  v.insert("c", 3);

  EXPECT_EQ(v.size(), 3u);

  std::set<std::string> keys;
  for (auto [k, val] : v.items())
    keys.insert(std::string(k));

  EXPECT_EQ(keys.size(), 3u);
  EXPECT_TRUE(keys.count("a"));
  EXPECT_TRUE(keys.count("b"));
  EXPECT_TRUE(keys.count("c")); // was missing before fix
}

TEST(ReproBugs2, ItemsIncludesInsertedKeysAfterErase) {
  Document doc;
  Value v = parse(doc, R"({"a":1,"b":2})");
  v.insert("c", 3);
  v.erase("a");

  // dump and size are correct
  EXPECT_EQ(v.size(), 2u);
  EXPECT_NE(v.dump().find("\"b\":2"), std::string::npos);
  EXPECT_NE(v.dump().find("\"c\":3"), std::string::npos);

  std::set<std::string> keys;
  for (auto [k, val] : v.items())
    keys.insert(std::string(k));

  EXPECT_EQ(keys.size(), 2u);
  EXPECT_FALSE(keys.count("a")); // erased
  EXPECT_TRUE(keys.count("b"));
  EXPECT_TRUE(keys.count("c")); // was missing before fix
}

TEST(ReproBugs2, ItemsValuesOfInsertedKeys) {
  Document doc;
  Value v = parse(doc, R"({"x":10})");
  v.insert("y", 20);
  v.insert("z", 30);

  std::map<std::string, int> got;
  for (auto [k, val] : v.items())
    got[std::string(k)] = val.as<int>();

  EXPECT_EQ(got.size(), 3u);
  EXPECT_EQ(got["x"], 10);
  EXPECT_EQ(got["y"], 20);
  EXPECT_EQ(got["z"], 30);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
