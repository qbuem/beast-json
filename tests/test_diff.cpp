// JSON diff + functional RFC 6902 applier (roadmap Tier 3).
// qbuem::diff(from, to) → an RFC 6902 patch; qbuem::apply_patch(doc, patch) →
// the patched document. The applier is purely functional (rebuild per op), so
// multi-op patches, array-index removal, pointer-token unescaping, and
// whole-document replacement all round-trip — unlike the in-place Value::patch().
#include <gtest/gtest.h>

#include "qbuem_json/qbuem_json.hpp"

#include <string>

namespace {
// apply_patch(A, diff(A,B)) must equal B (compared canonically).
void roundtrip(const char *a, const char *b) {
  std::string patch = qbuem::diff(std::string_view(a), std::string_view(b));
  std::string got = qbuem::apply_patch(std::string_view(a), patch);
  EXPECT_EQ(qbuem::canonicalize(std::string_view(got)),
            qbuem::canonicalize(std::string_view(b)))
      << "A=" << a << " B=" << b << " patch=" << patch;
}
} // namespace

TEST(Diff, IdenticalIsEmptyPatch) {
  EXPECT_EQ(qbuem::diff(std::string_view("{\"a\":1}"), std::string_view("{\"a\":1}")), "[]");
  EXPECT_EQ(qbuem::diff(std::string_view("[1,2]"), std::string_view("[1,2]")), "[]");
}

TEST(Diff, NumbersCompareByValue) {
  EXPECT_EQ(qbuem::diff(std::string_view("{\"a\":1}"), std::string_view("{\"a\":1.0}")), "[]");
}

TEST(Diff, LargeIntegersCompareExactly) {
  // 2^53 vs 2^53+1 must be reported different (a double compare would equate them).
  EXPECT_NE(qbuem::diff(std::string_view("{\"n\":9007199254740993}"),
                        std::string_view("{\"n\":9007199254740992}")),
            "[]");
  roundtrip("{\"n\":9007199254740993}", "{\"n\":9007199254740992}");
}

TEST(Diff, KeysMatchByDecodedName) {
  // "a" and "a" are the same logical key → no diff.
  EXPECT_EQ(qbuem::diff(std::string_view("{\"a\":1}"), std::string_view("{\"\\u0061\":1}")), "[]");
  // same key, different value → a replace, not remove+add.
  EXPECT_EQ(qbuem::diff(std::string_view("{\"x\":1}"), std::string_view("{\"\\u0078\":2}")),
            "[{\"op\":\"replace\",\"path\":\"/x\",\"value\":2}]");
}

TEST(Diff, ObjectAddRemoveReplace) {
  roundtrip("{\"a\":1}", "{\"a\":2}");
  roundtrip("{\"a\":1}", "{\"a\":1,\"b\":2}");
  roundtrip("{\"a\":1,\"b\":2}", "{\"a\":1}");
  roundtrip("{\"o\":{\"x\":1,\"y\":2}}", "{\"o\":{\"x\":9,\"z\":3}}");
}

TEST(Diff, TypeChanges) {
  roundtrip("{\"a\":1}", "{\"a\":[1,2]}");
  roundtrip("{\"a\":\"s\"}", "{\"a\":true}");
  roundtrip("{\"a\":1}", "[1,2]");   // container-root replace
  roundtrip("42", "{\"a\":1}");
}

TEST(Diff, Arrays) {
  roundtrip("{\"n\":[1,2,3]}", "{\"n\":[1,9,3]}");
  roundtrip("{\"n\":[1,2]}", "{\"n\":[1,2,3,4]}");   // grow
  roundtrip("{\"n\":[1,2,3,4]}", "{\"n\":[1,2]}");   // shrink
  roundtrip("[{\"t\":1},{\"t\":2}]", "[{\"t\":1},{\"t\":3},{\"t\":4}]");
  roundtrip("[1,2,3]", "[]");
  roundtrip("[]", "[1,2]");
}

TEST(Diff, DeepMultiOp) {
  roundtrip("{\"a\":{\"b\":{\"c\":[1,2,{\"d\":4}]}}}",
            "{\"a\":{\"b\":{\"c\":[1,5,{\"d\":4,\"e\":6}]}}}");
}

TEST(Diff, EscapedKeys) {
  roundtrip("{\"a/b\":1,\"c~d\":2}", "{\"a/b\":9,\"c~d\":2}");
  EXPECT_EQ(qbuem::diff(std::string_view("{\"a/b\":1}"), std::string_view("{\"a/b\":9}")),
            "[{\"op\":\"replace\",\"path\":\"/a~1b\",\"value\":9}]");
}

TEST(Diff, EmptyContainers) {
  roundtrip("{}", "{\"a\":1}");
  roundtrip("{\"a\":1}", "{}");
}

TEST(ApplyPatch, AllOpTypes) {
  EXPECT_EQ(qbuem::apply_patch(std::string_view("{\"a\":1}"),
                               std::string_view(R"([{"op":"add","path":"/b","value":2}])")),
            "{\"a\":1,\"b\":2}");
  EXPECT_EQ(qbuem::apply_patch(std::string_view("{\"n\":[1,2,3]}"),
                               std::string_view(R"([{"op":"remove","path":"/n/1"}])")),
            "{\"n\":[1,3]}");
  EXPECT_EQ(qbuem::apply_patch(std::string_view("{\"a\":1}"),
                               std::string_view(R"([{"op":"replace","path":"/a","value":9}])")),
            "{\"a\":9}");
  EXPECT_EQ(qbuem::apply_patch(std::string_view("{\"a\":1,\"b\":2}"),
                               std::string_view(R"([{"op":"move","from":"/a","path":"/c"}])")),
            "{\"b\":2,\"c\":1}");
  EXPECT_EQ(qbuem::apply_patch(std::string_view("{\"a\":1}"),
                               std::string_view(R"([{"op":"copy","from":"/a","path":"/b"}])")),
            "{\"a\":1,\"b\":1}");
}

TEST(ApplyPatch, TestOpPassesAndFails) {
  EXPECT_EQ(qbuem::apply_patch(std::string_view("{\"a\":1}"),
                               std::string_view(R"([{"op":"test","path":"/a","value":1}])")),
            "{\"a\":1}");
  EXPECT_THROW((void)qbuem::apply_patch(std::string_view("{\"a\":1}"),
                                        std::string_view(R"([{"op":"test","path":"/a","value":2}])")),
               std::runtime_error);
}

TEST(ApplyPatch, FailedOpThrows) {
  EXPECT_THROW((void)qbuem::apply_patch(std::string_view("{\"a\":1}"),
                                        std::string_view(R"([{"op":"remove","path":"/nope"}])")),
               std::runtime_error);
  EXPECT_THROW((void)qbuem::apply_patch(std::string_view("{}"),
                                        std::string_view("not-an-array")),
               qbuem::parse_error);
}
