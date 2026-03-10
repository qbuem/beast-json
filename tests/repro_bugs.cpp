#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <iostream>

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

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
