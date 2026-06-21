// NDJSON / JSON Lines (roadmap Tier 1): one JSON value per line, decoded one
// record at a time (bounded memory). Blank lines skipped, CRLF trimmed, last
// line need not be newline-terminated.
#include <gtest/gtest.h>

#include "qbuem_json/qbuem_json.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct Rec { int64_t id; std::string name; };
QBUEM_JSON_FIELDS(Rec, id, name)

TEST(Ndjson, ReadCollectsRecords) {
  const std::string nd =
      "{\"id\":1,\"name\":\"a\"}\n"
      "{\"id\":2,\"name\":\"b\"}\n"
      "{\"id\":3,\"name\":\"c\"}\n";
  auto recs = qbuem::read_lines<Rec>(nd);
  ASSERT_EQ(recs.size(), 3u);
  EXPECT_EQ(recs[0].id, 1);
  EXPECT_EQ(recs[1].name, "b");
  EXPECT_EQ(recs[2].id, 3);
}

TEST(Ndjson, BlankLinesAndCrlfAndNoFinalNewline) {
  const std::string nd =
      "{\"id\":1,\"name\":\"a\"}\n"
      "\r\n"                          // blank CRLF
      "{\"id\":2,\"name\":\"b\"}\r\n" // CRLF-terminated record
      "   \n"                         // whitespace-only
      "{\"id\":3,\"name\":\"c\"}";    // no trailing newline
  auto recs = qbuem::read_lines<Rec>(nd);
  ASSERT_EQ(recs.size(), 3u);
  EXPECT_EQ(recs[1].name, "b");
  EXPECT_EQ(recs[2].name, "c");
}

TEST(Ndjson, CallbackFormIsBoundedAndOrdered) {
  const std::string nd = "{\"id\":10,\"name\":\"x\"}\n{\"id\":20,\"name\":\"y\"}\n";
  int64_t sum = 0;
  std::vector<std::string> order;
  qbuem::read_lines<Rec>(nd, [&](Rec&& r) { sum += r.id; order.push_back(r.name); });
  EXPECT_EQ(sum, 30);
  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[0], "x");
  EXPECT_EQ(order[1], "y");
}

TEST(Ndjson, ScalarAndArrayRecords) {
  auto nums = qbuem::read_lines<int>("1\n2\n3\n");
  EXPECT_EQ(nums, (std::vector<int>{1, 2, 3}));
  auto rows = qbuem::read_lines<std::vector<int>>("[1,2]\n[3,4,5]\n");
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[1].size(), 3u);
}

TEST(Ndjson, WriteLinesRoundTrip) {
  std::vector<Rec> recs{ {1, "a"}, {2, "b"}, {3, "c"} };
  std::string out = qbuem::write_lines(recs);
  EXPECT_EQ(out, "{\"id\":1,\"name\":\"a\"}\n{\"id\":2,\"name\":\"b\"}\n{\"id\":3,\"name\":\"c\"}\n");
  auto back = qbuem::read_lines<Rec>(out);
  ASSERT_EQ(back.size(), 3u);
  EXPECT_EQ(back[2].name, "c");
}

TEST(Ndjson, EmptyAndBlankOnlyInput) {
  EXPECT_TRUE(qbuem::read_lines<Rec>("").empty());
  EXPECT_TRUE(qbuem::read_lines<Rec>("\n\n  \n\r\n").empty());
}

TEST(Ndjson, MalformedRecordThrows) {
  EXPECT_THROW((void)qbuem::read_lines<Rec>("{\"id\":1}\n{bad}\n"),
               qbuem::parse_error);
}
