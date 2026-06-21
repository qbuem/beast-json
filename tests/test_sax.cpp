// SAX-style event visitor (roadmap Tier 2): qbuem::visit / sax_parse walk a
// parsed document depth-first, emitting an event per node to a handler derived
// from qbuem::sax_handler. Static dispatch (no virtuals); a handler returning
// false aborts the walk.
#include <gtest/gtest.h>

#include "qbuem_json/qbuem_json.hpp"

#include <cstdint>
#include <string>

namespace {
// Serializes the event stream to a compact log for exact-match assertions.
struct Recorder : qbuem::sax_handler {
  std::string log;
  bool start_object() { log += "{"; return true; }
  bool end_object(size_t n) { log += "}" + std::to_string(n); return true; }
  bool start_array() { log += "["; return true; }
  bool end_array(size_t n) { log += "]" + std::to_string(n); return true; }
  bool key(std::string_view k) { log += "k:"; log.append(k); log += ";"; return true; }
  bool string(std::string_view s) { log += "s:"; log.append(s); log += ";"; return true; }
  bool integer(int64_t i) { log += "i:" + std::to_string(i) + ";"; return true; }
  bool real(double) { log += "r;"; return true; }
  bool boolean(bool b) { log += b ? "T;" : "F;"; return true; }
  bool null() { log += "N;"; return true; }
};
} // namespace

TEST(Sax, FullEventStream) {
  Recorder r;
  ASSERT_TRUE(qbuem::sax_parse(R"({"a":1,"b":[true,null,"x"],"c":2.5})", r));
  EXPECT_EQ(r.log, "{k:a;i:1;k:b;[T;N;s:x;]3k:c;r;}3");
}

TEST(Sax, VisitParsedValue) {
  qbuem::Document d;
  auto root = qbuem::parse(d, "[1,2,3]");
  Recorder r;
  EXPECT_TRUE(qbuem::visit(root, r));
  EXPECT_EQ(r.log, "[i:1;i:2;i:3;]3");
}

TEST(Sax, MemberAndElementCounts) {
  Recorder r;
  qbuem::sax_parse(R"({"x":{},"y":[1,2]})", r);
  EXPECT_EQ(r.log, "{k:x;{}0k:y;[i:1;i:2;]2}2");
}

TEST(Sax, DefaultHandlerIsNoOp) {
  // A handler that overrides nothing walks without error and counts via return.
  qbuem::sax_handler h;
  EXPECT_TRUE(qbuem::sax_parse(R"({"a":[1,{"b":2}],"c":null})", h));
}

TEST(Sax, AbortStopsTheWalk) {
  struct Aborter : qbuem::sax_handler {
    int keys = 0;
    bool key(std::string_view) { ++keys; return false; } // abort on first key
  } a;
  EXPECT_FALSE(qbuem::sax_parse(R"({"a":1,"b":2})", a));
  EXPECT_EQ(a.keys, 1);
}

TEST(Sax, AbortInsideNestedArray) {
  struct Counter : qbuem::sax_handler {
    int ints = 0;
    bool integer(int64_t) { ++ints; return ints < 2; } // abort on 2nd integer
  } c;
  EXPECT_FALSE(qbuem::sax_parse("[1,2,3,4]", c));
  EXPECT_EQ(c.ints, 2);
}

TEST(Sax, TopLevelScalars) {
  Recorder r1; qbuem::sax_parse("42", r1);  EXPECT_EQ(r1.log, "i:42;");
  Recorder r2; qbuem::sax_parse("true", r2); EXPECT_EQ(r2.log, "T;");
  Recorder r3; qbuem::sax_parse("null", r3); EXPECT_EQ(r3.log, "N;");
  Recorder r4; qbuem::sax_parse("\"hi\"", r4); EXPECT_EQ(r4.log, "s:hi;");
}

TEST(Sax, InvalidInputThrows) {
  Recorder r;
  EXPECT_THROW((void)qbuem::sax_parse("{bad}", r), qbuem::parse_error);
}
