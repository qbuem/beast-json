#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>
#include <string_view>

using namespace beast::json;

// lazy::parse_reuse accepts trailing commas
TEST(TrailingCommas, ArrayTrailingCommaAccepted) {
  lazy::DocumentView doc;
  EXPECT_NO_THROW(lazy::parse_reuse(doc, "[1, 2, 3, ]"));
  EXPECT_NO_THROW(lazy::parse_reuse(doc, "[\"a\", ]"));
  EXPECT_NO_THROW(lazy::parse_reuse(doc, "[[], ]"));
}

TEST(TrailingCommas, ObjectTrailingCommaAccepted) {
  lazy::DocumentView doc;
  EXPECT_NO_THROW(lazy::parse_reuse(doc, "{\"a\": 1, }"));
  EXPECT_NO_THROW(lazy::parse_reuse(doc, "{\"a\": {\"b\": 1, }, }"));
}

TEST(TrailingCommas, LazyParserAcceptsTrailingComma) {
  std::string arr = "[1, 2, ]";
  lazy::DocumentView doc;
  EXPECT_NO_THROW(lazy::parse_reuse(doc, arr));

  std::string obj = "{\"k\": 1, }";
  EXPECT_NO_THROW(lazy::parse_reuse(doc, obj));
}

// Round-trip: dump() emits compact JSON without trailing commas
TEST(TrailingCommas, RoundTripPreservesStructure) {
  lazy::DocumentView doc;
  auto v = lazy::parse_reuse(doc, "[1,2,]");
  std::string out = v.dump();
  EXPECT_EQ(out, "[1,2]");
}
