#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace qbuem;

// Helper: attempt dom parse, return true on success
static bool dom_ok(std::string_view j) {
  try {
    Document doc;
    parse(doc, j);
    return true;
  } catch (const std::runtime_error &) {
    return false;
  }
}

// Unterminated containers: depth != 0 at end → parse error
TEST(ErrorHandling, UnterminatedContainers) {
  EXPECT_FALSE(dom_ok("["));
  EXPECT_FALSE(dom_ok("{"));
  EXPECT_FALSE(dom_ok("[1, 2"));
  EXPECT_FALSE(dom_ok("{\"a\":"));
  EXPECT_FALSE(dom_ok("[[["));
}

// Invalid literals: prefix-length check fails
TEST(ErrorHandling, InvalidLiterals) {
  EXPECT_FALSE(dom_ok("tru"));
  EXPECT_FALSE(dom_ok("truth"));
  EXPECT_FALSE(dom_ok("fal"));
  EXPECT_FALSE(dom_ok("falsy"));
  EXPECT_FALSE(dom_ok("nul"));
  EXPECT_FALSE(dom_ok("nulls"));
}

// Empty input: skip_to_action returns 0 → parse error
TEST(ErrorHandling, EmptyInput) {
  EXPECT_FALSE(dom_ok(""));
  EXPECT_FALSE(dom_ok("   "));
}

// Unrecognized value-start chars
TEST(ErrorHandling, UnrecognizedValueChars) {
  EXPECT_FALSE(dom_ok("[!]"));
  EXPECT_FALSE(dom_ok("[?]"));
  EXPECT_FALSE(dom_ok("[&]"));
}

// Unbalanced depth: unclosed containers
TEST(ErrorHandling, UnbalancedDepth) {
  EXPECT_FALSE(dom_ok("[1,2"));
  EXPECT_FALSE(dom_ok("{\"a\":1"));
  EXPECT_FALSE(dom_ok("{\"key\":\"value\""));
}

// ── Rich error context: line / column / caret (v1.3) ─────────────────────────

// Catch a parse_error and return it, or nullopt-style sentinel via flag.
static bool catch_parse_error(std::string_view j, qbuem::parse_error &out) {
  try {
    Document doc;
    parse(doc, j);
    return false;
  } catch (const qbuem::parse_error &e) {
    out = e;
    return true;
  }
}

TEST(ErrorContext, LineColumnOnMultilineInput) {
  // Error (@) sits on the 3rd line.
  const std::string j = "{\n  \"a\": 1,\n  \"b\": @\n}";
  qbuem::parse_error e("", 0);
  ASSERT_TRUE(catch_parse_error(j, e));
  EXPECT_EQ(e.line(), 3u);          // newline count is deterministic
  EXPECT_GE(e.column(), 1u);
  EXPECT_LT(e.offset(), j.size());
}

TEST(ErrorContext, SingleLineIsLineOne) {
  qbuem::parse_error e("", 0);
  ASSERT_TRUE(catch_parse_error("[1, 2, @]", e));
  EXPECT_EQ(e.line(), 1u);
  EXPECT_GE(e.column(), 1u);
}

TEST(ErrorContext, OffsetStillReported) {
  // Backward-compat: offset() remains valid.
  qbuem::parse_error e("", 0);
  ASSERT_TRUE(catch_parse_error("{\"x\": tru}", e));
  EXPECT_GT(e.offset(), 0u);
}

TEST(ErrorContext, WhatMentionsLineColumn) {
  qbuem::parse_error e("", 0);
  ASSERT_TRUE(catch_parse_error("{\n  bad\n}", e));
  const std::string what = e.what();
  EXPECT_NE(what.find("line"), std::string::npos);
  EXPECT_NE(what.find("column"), std::string::npos);
}

TEST(ErrorContext, FormatErrorShowsCaret) {
  const std::string j = "{\n  \"a\": 1,\n  \"b\": @\n}";
  qbuem::parse_error e("", 0);
  ASSERT_TRUE(catch_parse_error(j, e));
  const std::string rendered = qbuem::format_error(e, j);
  EXPECT_NE(rendered.find('^'), std::string::npos);     // caret present
  EXPECT_NE(rendered.find("\"b\""), std::string::npos);  // offending line shown
}

TEST(ErrorContext, FormatErrorNoLocationFallsBack) {
  // A bare offset-only error (line == 0) renders as what(), no caret.
  qbuem::parse_error e("synthetic", 5);
  EXPECT_EQ(qbuem::format_error(e, "whatever"), std::string("synthetic"));
}

TEST(ErrorContext, FormatErrorWindowsLongLine) {
  // A very long single line must not crash and must still carry a caret.
  std::string j = "[";
  j.append(400, '1');
  j += " @]"; // invalid token far along the line
  qbuem::parse_error e("", 0);
  ASSERT_TRUE(catch_parse_error(j, e));
  const std::string rendered = qbuem::format_error(e, j);
  EXPECT_NE(rendered.find('^'), std::string::npos);
  EXPECT_LT(rendered.size(), j.size());                 // windowed, not the whole blob
}
