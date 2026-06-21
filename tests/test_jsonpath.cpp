// JSONPath (roadmap Tier 2, RFC 9535 structural selectors): root, member, index
// (incl. negative), wildcard, recursive descent, slices, unions. Filters are not
// supported (a query with one throws).
#include <gtest/gtest.h>

#include "qbuem_json/qbuem_json.hpp"

#include <string>
#include <vector>

namespace {
const char *kDoc = R"({
  "store": {
    "book": [
      {"category":"ref","author":"Nigel","title":"Sayings","price":8.95},
      {"category":"fiction","author":"Waugh","title":"Sword","price":12.99},
      {"category":"fiction","author":"Melville","title":"Moby","price":8.99}
    ],
    "bicycle": {"color":"red","price":19.95}
  },
  "nums": [10,20,30,40,50]
})";
} // namespace

struct JsonPath : ::testing::Test {
  qbuem::Document doc;
  qbuem::Value root;
  void SetUp() override { root = qbuem::parse(doc, kDoc); }
};

TEST_F(JsonPath, MemberAndIndex) {
  auto r = qbuem::query(root, "$.store.book[0].title");
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].as<std::string_view>(), "Sayings");
}

TEST_F(JsonPath, NegativeIndex) {
  auto r = qbuem::query(root, "$.store.book[-1].author");
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].as<std::string_view>(), "Melville");
}

TEST_F(JsonPath, WildcardArrayAndObject) {
  EXPECT_EQ(qbuem::query(root, "$.store.book[*].author").size(), 3u);
  EXPECT_EQ(qbuem::query(root, "$.store.*").size(), 2u); // book + bicycle
}

TEST_F(JsonPath, RecursiveDescent) {
  EXPECT_EQ(qbuem::query(root, "$..price").size(), 4u); // 3 books + bicycle
  EXPECT_EQ(qbuem::query(root, "$..author").size(), 3u);
  EXPECT_FALSE(qbuem::query(root, "$..*").empty());
}

TEST_F(JsonPath, BracketNameWithQuotes) {
  auto r = qbuem::query(root, "$['store']['bicycle']['color']");
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].as<std::string_view>(), "red");
}

TEST_F(JsonPath, Slices) {
  auto a = qbuem::query(root, "$.nums[1:4]");
  ASSERT_EQ(a.size(), 3u);
  EXPECT_EQ(a[0].as<int>(), 20);
  EXPECT_EQ(a[2].as<int>(), 40);
  EXPECT_EQ(qbuem::query(root, "$.nums[:2]").size(), 2u);
  auto step = qbuem::query(root, "$.nums[::2]");
  ASSERT_EQ(step.size(), 3u);
  EXPECT_EQ(step[2].as<int>(), 50);
  auto neg = qbuem::query(root, "$.nums[-2:]");
  ASSERT_EQ(neg.size(), 2u);
  EXPECT_EQ(neg[0].as<int>(), 40);
  auto rev = qbuem::query(root, "$.nums[::-1]");
  ASSERT_EQ(rev.size(), 5u);
  EXPECT_EQ(rev[0].as<int>(), 50);
  EXPECT_EQ(rev[4].as<int>(), 10);
}

TEST_F(JsonPath, Union) {
  auto r = qbuem::query(root, "$.nums[0, 2, 4]");
  ASSERT_EQ(r.size(), 3u);
  EXPECT_EQ(r[0].as<int>(), 10);
  EXPECT_EQ(r[2].as<int>(), 50);
}

TEST_F(JsonPath, RootAndNoMatch) {
  EXPECT_EQ(qbuem::query(root, "$").size(), 1u);
  EXPECT_TRUE(qbuem::query(root, "$.store.nope").empty());
  EXPECT_TRUE(qbuem::query(root, "$.nums[99]").empty());
  EXPECT_TRUE(qbuem::query(root, "$.nums.author").empty()); // type mismatch
}

TEST_F(JsonPath, MalformedQueryThrows) {
  EXPECT_THROW((void)qbuem::query(root, "store.x"), qbuem::parse_error);   // no '$'
  EXPECT_THROW((void)qbuem::query(root, "$.a[1"), qbuem::parse_error);     // unterminated
  EXPECT_THROW((void)qbuem::query(root, "$.a['x"), qbuem::parse_error);    // unterminated string
  EXPECT_THROW((void)qbuem::query(root, "$.a[]"), qbuem::parse_error);     // empty selector
}

TEST_F(JsonPath, FilterSelectorsAreRejected) {
  EXPECT_THROW((void)qbuem::query(root, "$.store.book[?@.price]"),
               qbuem::parse_error);
}

TEST_F(JsonPath, OutOfRangeIndexRejectedNotWrapped) {
  // An index that overflows int64 is a malformed query (RFC 9535 limits index to
  // the I-JSON range) — it must be rejected cleanly, not wrap via overflow.
  EXPECT_THROW((void)qbuem::query(root, "$.nums[99999999999999999999999999]"),
               qbuem::parse_error);
  // A large-but-representable index simply doesn't match.
  EXPECT_TRUE(qbuem::query(root, "$.nums[1000000000]").empty());
}

TEST_F(JsonPath, JsonpathAliasMatchesQuery) {
  // qbuem::jsonpath is an exact alias for qbuem::query (the RFC 9535 spelling).
  auto a = qbuem::jsonpath(root, "$.store.book[*].author");
  auto b = qbuem::query(root, "$.store.book[*].author");
  ASSERT_EQ(a.size(), b.size());
  ASSERT_EQ(a.size(), 3u);
  for (size_t i = 0; i < a.size(); ++i)
    EXPECT_EQ(a[i].as<std::string_view>(), b[i].as<std::string_view>());
  EXPECT_THROW((void)qbuem::jsonpath(root, "store.x"), qbuem::parse_error);
}
