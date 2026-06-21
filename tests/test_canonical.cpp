// Canonical JSON (roadmap Tier 1): qbuem::canonicalize produces a deterministic
// serialization — sorted keys, no whitespace, shortest numbers, minimal escaping,
// -0 normalized — so the same logical document hashes to the same bytes.
#include <gtest/gtest.h>

#include "qbuem_json/qbuem_json.hpp"

#include <string>

TEST(Canonical, SortsKeysAndStripsWhitespace) {
  EXPECT_EQ(qbuem::canonicalize("{ \"b\":1, \"a\":2, \"c\":[3, 2] }"),
            "{\"a\":2,\"b\":1,\"c\":[3,2]}");
}

TEST(Canonical, ShortensNumbersAndNormalizesNegativeZero) {
  EXPECT_EQ(qbuem::canonicalize("{\"x\":1.50,\"y\":1e2,\"z\":-0.0}"),
            "{\"x\":1.5,\"y\":100,\"z\":0}");
}

TEST(Canonical, RecursesIntoNestedObjects) {
  EXPECT_EQ(qbuem::canonicalize("{\"o\":{\"z\":1,\"a\":{\"y\":1,\"b\":2}}}"),
            "{\"o\":{\"a\":{\"b\":2,\"y\":1},\"z\":1}}");
}

TEST(Canonical, DecodesAndMinimallyReescapesStrings) {
  // A → A, escaped newline stays \n.
  EXPECT_EQ(qbuem::canonicalize("{\"k\":\"\\u0041\\nB\"}"), "{\"k\":\"A\\nB\"}");
}

TEST(Canonical, NormalizesEscapedKeysAndSortsByDecodedValue) {
  EXPECT_EQ(qbuem::canonicalize("{\"\\u0062\":1,\"a\":2}"), "{\"a\":2,\"b\":1}");
}

TEST(Canonical, IsDeterministicAcrossKeyOrder) {
  EXPECT_EQ(qbuem::canonicalize("{\"a\":1,\"b\":2,\"c\":3}"),
            qbuem::canonicalize("{\"c\":3,\"a\":1,\"b\":2}"));
}

TEST(Canonical, IsIdempotent) {
  const std::string once = qbuem::canonicalize("{\"b\":[1,2],\"a\":{\"d\":4,\"c\":3}}");
  EXPECT_EQ(qbuem::canonicalize(once), once);
}

TEST(Canonical, ArraysPreserveOrderAndScalarsRoundTrip) {
  EXPECT_EQ(qbuem::canonicalize("[3, 1, 2]"), "[3,1,2]");
  EXPECT_EQ(qbuem::canonicalize("  \"hi\" "), "\"hi\"");
  EXPECT_EQ(qbuem::canonicalize("null"), "null");
}

TEST(Canonical, InvalidInputThrows) {
  EXPECT_THROW((void)qbuem::canonicalize("{\"a\":}"), qbuem::parse_error);
}

TEST(Canonical, ToVariantAppendsToBuffer) {
  // canonicalize_to APPENDS to the caller's buffer (reuses its content), and
  // produces the same bytes the string-returning overload would.
  std::string buf = "PREFIX:";
  qbuem::canonicalize_to(buf, "{ \"b\":1, \"a\":2 }");
  EXPECT_EQ(buf, "PREFIX:{\"a\":2,\"b\":1}");

  std::string fresh;
  qbuem::canonicalize_to(fresh, "{\"x\":1.50}");
  EXPECT_EQ(fresh, qbuem::canonicalize("{\"x\":1.50}"));
}
