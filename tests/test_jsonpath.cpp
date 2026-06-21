// JSONPath (roadmap Tier 2, RFC 9535): root, member, index (incl. negative),
// wildcard, recursive descent, slices, unions, and filter selectors `[?...]`
// (comparisons, existence tests, &&/||/! with parens, length()/count()/value()).
// The I-Regexp functions match()/search() are intentionally unsupported (throw).
#include <gtest/gtest.h>

#include "qbuem_json/qbuem_json.hpp"

#include <string>
#include <vector>

namespace {
const char *kDoc = R"({
  "store": {
    "book": [
      {"category":"ref","author":"Nigel","title":"Sayings","price":8.95},
      {"category":"fiction","author":"Waugh","title":"Sword","price":12.99,"isbn":"x"},
      {"category":"fiction","author":"Melville","title":"Moby","price":8.99,"isbn":"y"}
    ],
    "bicycle": {"color":"red","price":19.95}
  },
  "nums": [10,20,30,40,50],
  "tags": ["a","bb","ccc"]
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

// ── Filter selectors (RFC 9535 §2.3.5) ──────────────────────────────────────

TEST_F(JsonPath, FilterComparisons) {
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.price<10]").size(), 2u);   // 8.95, 8.99
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.price>=10]").size(), 1u);  // 12.99
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.price==8.99]").size(), 1u);
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.price!=8.99]").size(), 2u);
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.category=='fiction']").size(), 2u);
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.category!='fiction']").size(), 1u);
  // string ordering is code-point lexicographic
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.author>'N']").size(), 2u); // Nigel, Waugh (Melville < N)
}

TEST_F(JsonPath, FilterExistence) {
  // 2 of 3 books carry an isbn.
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.isbn]").size(), 2u);
  EXPECT_EQ(qbuem::query(root, "$.store.book[?!@.isbn]").size(), 1u);
  // absolute existence test: true for every element if the root key exists.
  EXPECT_EQ(qbuem::query(root, "$.store.book[?$.store]").size(), 3u);
}

TEST_F(JsonPath, FilterLogicalAndParens) {
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.category=='fiction' && @.price<10]").size(), 1u);
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.price<9 || @.price>15]").size(), 2u); // 8.95, 8.99
  EXPECT_EQ(qbuem::query(root, "$.store.book[?(@.category=='ref' || @.isbn) && @.price<13]").size(), 3u);
  EXPECT_EQ(qbuem::query(root, "$.store.book[?!(@.price<10)]").size(), 1u);
}

TEST_F(JsonPath, FilterOnScalarArrayUsesCurrentNode) {
  // `@` is each array element directly.
  EXPECT_EQ(qbuem::query(root, "$.nums[?@>25]").size(), 3u);          // 30,40,50
  EXPECT_EQ(qbuem::query(root, "$.nums[?@>=20 && @<=40]").size(), 3u); // 20,30,40
}

TEST_F(JsonPath, FilterDescendant) {
  // every object anywhere with price < 10 → the two cheap books.
  EXPECT_EQ(qbuem::query(root, "$..[?@.price<10]").size(), 2u);
}

TEST_F(JsonPath, FilterLengthFunction) {
  // tags = ["a","bb","ccc"]; keep those with ≥2 characters.
  EXPECT_EQ(qbuem::query(root, "$.tags[?length(@)>=2]").size(), 2u);
  // length of an object/array also works.
  EXPECT_EQ(qbuem::query(root, "$.store.book[?length(@)>4]").size(), 2u); // the 5-key books
}

TEST_F(JsonPath, FilterCountAndValueFunctions) {
  // count() over a (possibly empty) relative query.
  EXPECT_EQ(qbuem::query(root, "$.store.book[?count(@.isbn)==1]").size(), 2u);
  EXPECT_EQ(qbuem::query(root, "$.store.book[?count(@.isbn)==0]").size(), 1u);
  // value() unwraps a singular nodelist for comparison.
  EXPECT_EQ(qbuem::query(root, "$.store.book[?value(@.price)<10]").size(), 2u);
}

TEST_F(JsonPath, FilterAbsoluteOperand) {
  // compare each book's price against an absolute singular query.
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.price==$.store.bicycle.price]").size(), 0u);
  EXPECT_EQ(qbuem::query(root, "$.store.book[?@.price<$.store.bicycle.price]").size(), 3u);
}

TEST_F(JsonPath, FilterNoMatchAndTypeMismatch) {
  EXPECT_TRUE(qbuem::query(root, "$.store.book[?@.price>1000]").empty());
  // ordering across mismatched types is always false (RFC 9535).
  EXPECT_TRUE(qbuem::query(root, "$.store.book[?@.category<10]").empty());
}

TEST_F(JsonPath, RegexFunctionsRejected) {
  // match()/search() require an I-Regexp engine — intentionally unsupported.
  EXPECT_THROW((void)qbuem::query(root, "$.store.book[?match(@.title,'.*')]"),
               qbuem::parse_error);
  EXPECT_THROW((void)qbuem::query(root, "$.store.book[?search(@.title,'M')]"),
               qbuem::parse_error);
}

TEST_F(JsonPath, DeeplyNestedFilterRejectedNotCrash) {
  // A pathologically nested filter must throw (depth cap), never overflow the
  // stack — query strings may come from untrusted sources.
  std::string q = "$.nums[?";
  q.append(700, '(');
  q += "@>1";
  q.append(700, ')');
  q += "]";
  EXPECT_THROW((void)qbuem::query(root, q), qbuem::parse_error);
}

TEST_F(JsonPath, MalformedFiltersThrow) {
  EXPECT_THROW((void)qbuem::query(root, "$.nums[?@ <]"), qbuem::parse_error);     // no rhs
  EXPECT_THROW((void)qbuem::query(root, "$.nums[?@ <> 1]"), qbuem::parse_error);  // bad op
  EXPECT_THROW((void)qbuem::query(root, "$.nums[?(@>1]"), qbuem::parse_error);    // unbalanced paren
  EXPECT_THROW((void)qbuem::query(root, "$.nums[?5]"), qbuem::parse_error);       // bare literal test
  EXPECT_THROW((void)qbuem::query(root, "$.book[?@.a[*]==1]"), qbuem::parse_error);// non-singular operand
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
