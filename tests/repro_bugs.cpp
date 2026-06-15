#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>

using namespace qbuem;

// Struct used by the Nexus (fuse) data-integrity tests below.
struct FuseRec { int a; long b; };
QBUEM_JSON_FIELDS(FuseRec, a, b)

// Struct with string fields to exercise the Nexus from_json_direct<string> path.
struct FuseStr { std::string s; std::optional<std::string> o; };
QBUEM_JSON_FIELDS(FuseStr, s, o)

// Struct with a vector to exercise the Nexus sequence-decoding loop.
struct FuseVec { std::vector<int> v; };
QBUEM_JSON_FIELDS(FuseVec, v)

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

// ── BUG-1: qbuem::parse_reuse not exposed in public facade ───────────────────
//
// qbuem::json::parse_reuse existed internally but qbuem::parse_reuse was not
// forwarded in the public qbuem:: namespace.
// Use explicit qualification to avoid ADL ambiguity with qbuem::json::parse_reuse.
TEST(ReproBugs2, ParseReusePublicFacade) {
  qbuem::Document doc;
  // First parse
  qbuem::Value v1 = qbuem::parse_reuse(doc, R"({"a":1})");
  EXPECT_TRUE(v1.is_object());
  EXPECT_EQ(v1["a"].as<int>(), 1);

  // Reuse same document handle for a second parse
  qbuem::Value v2 = qbuem::parse_reuse(doc, R"({"b":2})");
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

// ── OBS-1: unset() reverts to original parsed value, NOT null reset ──────────
//
// unset() removes the mutation overlay so as<T>() and type_name() return the
// original parsed value/type. It does NOT reset the value to null.
TEST(Observations, UnsetRevertsToOriginalValue) {
  Document doc;
  Value root = parse(doc, R"({"n":42,"s":"hello","flag":true})");

  // Mutate, then revert
  root["n"].set(999);
  EXPECT_EQ(root["n"].as<int>(), 999);
  root["n"].unset();
  EXPECT_EQ(root["n"].as<int>(), 42); // original value restored

  root["s"].set("world");
  EXPECT_EQ(root["s"].as<std::string>(), "world");
  root["s"].unset();
  EXPECT_EQ(root["s"].as<std::string>(), "hello"); // original restored

  root["flag"].set(false);
  EXPECT_FALSE(root["flag"].as<bool>());
  root["flag"].unset();
  EXPECT_TRUE(root["flag"].as<bool>()); // original true restored
}

TEST(Observations, UnsetTypeNameRetainsOriginalType) {
  Document doc;
  Value root = parse(doc, R"({"b":false,"n":10})");

  // Set bool to bool — after unset, still bool (original type preserved)
  root["b"].set(true);
  EXPECT_EQ(root["b"].type_name(), "bool");
  root["b"].unset();
  // type_name() returns original parsed type, not "null"
  EXPECT_EQ(root["b"].type_name(), "bool");
  EXPECT_FALSE(root["b"].as<bool>()); // original value: false

  // Set int to int — after unset, still int
  root["n"].set(99);
  root["n"].unset();
  EXPECT_EQ(root["n"].type_name(), "int");
  EXPECT_EQ(root["n"].as<int>(), 10);
}

// ── OBS-2: operator[] returns invalid Value on miss, does NOT throw ───────────
//
// The API documentation previously said "throws on miss (like STL at())".
// The actual behavior is non-throwing: a miss returns an invalid Value{}.
// Only calling as<T>() on that invalid Value subsequently throws.
TEST(Observations, SubscriptMissReturnsInvalidNotThrow) {
  Document doc;
  Value root = parse(doc, R"({"a":1})");

  // Object miss — must not throw; must return invalid Value
  Value miss_obj = root["nonexistent"];
  EXPECT_FALSE(miss_obj.is_valid());

  // Array miss — must not throw; must return invalid Value
  Value arr = parse(doc, "[1,2,3]");
  Value miss_arr = arr[99];
  EXPECT_FALSE(miss_arr.is_valid());

  // Chained miss — each level is a no-throw invalid Value
  Value deep = root["x"]["y"]["z"];
  EXPECT_FALSE(deep.is_valid());
}

TEST(Observations, AsOnInvalidValueThrows) {
  Document doc;
  Value root = parse(doc, R"({"a":1})");

  // operator[] itself does not throw, but as<T>() on the result does
  Value miss = root["nonexistent"];
  EXPECT_FALSE(miss.is_valid());
  EXPECT_THROW(miss.as<int>(), std::runtime_error);
  EXPECT_THROW(miss.as<std::string>(), std::runtime_error);
}

// ── OBS-3: as_array<T>() throws on type mismatch; try_as_array<T>() is safe ──
//
// as_array<T>() throws std::runtime_error when an element cannot be converted
// to T. try_as_array<T>() returns std::optional<T> and never throws.
TEST(Observations, AsArrayThrowsOnTypeMismatch) {
  Document doc;
  Value v = parse(doc, R"([1,"two",3])");

  bool threw = false;
  try {
    for (int x : v.as_array<int>()) {
      (void)x; // suppress unused warning
    }
  } catch (const std::runtime_error &) {
    threw = true;
  }
  EXPECT_TRUE(threw); // throws when reaching "two"
}

TEST(Observations, TryAsArrayHandlesMixedTypes) {
  Document doc;
  Value v = parse(doc, R"([1,"two",3])");

  std::vector<int> ints;
  for (auto maybe : v.try_as_array<int>()) {
    if (maybe.has_value())
      ints.push_back(maybe.value());
  }

  // Only integer elements are collected; "two" silently skipped
  ASSERT_EQ(ints.size(), 2u);
  EXPECT_EQ(ints[0], 1);
  EXPECT_EQ(ints[1], 3);
}

TEST(Observations, TryAsArrayNeverThrows) {
  Document doc;
  Value v = parse(doc, R"([true,42,"text",null,3.14])");

  // Mixed type array — try_as_array<int> never throws regardless of types
  EXPECT_NO_THROW({
    for ([[maybe_unused]] auto maybe : v.try_as_array<int>()) {
    }
  });
}

// ── SEC: deeply-nested input must be rejected, never overflow the stack ──────
// Before the fix, the tape Parser wrote cstate_stack_[depth_] with no bound
// check (stack-buffer-overflow) and the RFC validator recursed unboundedly
// (SIGSEGV).  These tests must not crash.
TEST(SecurityHardening, DeepNestedArrayRejectedNoCrash) {
  std::string json(5000, '[');
  json.append(5000, ']');
  Document doc;
  EXPECT_THROW(parse(doc, json), std::runtime_error);
}

TEST(SecurityHardening, DeepNestedObjectRejectedNoCrash) {
  std::string json;
  for (int i = 0; i < 3000; ++i)
    json += "{\"a\":";
  json += "1";
  json.append(3000, '}');
  Document doc;
  EXPECT_THROW(parse(doc, json), std::runtime_error);
}

TEST(SecurityHardening, ShallowNestingStillParses) {
  std::string json(500, '[');
  json.append(500, ']');
  Document doc;
  EXPECT_NO_THROW(parse(doc, json));
}

TEST(SecurityHardening, ValidatorRejectsDeepNesting) {
  std::string json(50000, '[');
  json.append(50000, ']');
  EXPECT_THROW(rfc8259::validate(json), std::runtime_error);
}

// ── INTEGRITY: tokens larger than the 16-bit length field were silently
// truncated (round-trip corruption).  They must now be rejected cleanly.
TEST(DataIntegrity, OversizeStringTokenRejected) {
  std::string big(70000, 'a');
  std::string json = "{\"k\":\"" + big + "\"}";
  Document doc;
  EXPECT_THROW(parse(doc, json), std::runtime_error);
}

TEST(DataIntegrity, SubMaxStringTokenRoundTrips) {
  std::string big(60000, 'a'); // < 65535: must parse and round-trip exactly
  std::string json = "{\"k\":\"" + big + "\"}";
  Document doc;
  Value r = parse(doc, json);
  EXPECT_EQ(r["k"].as<std::string>().size(), 60000u);
  EXPECT_EQ(r.dump(), json);
}

TEST(DataIntegrity, OversizeNumberTokenRejected) {
  std::string num(70000, '1');
  std::string json = "[" + num + "]";
  Document doc;
  EXPECT_THROW(parse(doc, json), std::runtime_error);
}

// ── INTEGRITY: insert()/set() must JSON-escape string content so the output
// is valid JSON.
TEST(DataIntegrity, InsertEscapesStringContent) {
  Document doc;
  Value root = parse(doc, "{}");
  root.insert("k", std::string_view("a\"b\\c\n\t"));
  // '"', '\\', '\n', '\t' must be escaped → output is exact valid JSON.
  EXPECT_EQ(root.dump(), R"({"k":"a\"b\\c\n\t"})");

  // Control characters (< 0x20) escape to \u00XX; output must re-parse cleanly.
  Value root2 = parse(doc, "{}");
  char ctrl[] = {(char)0x01, (char)0x1f};
  root2.insert("c", std::string_view(ctrl, sizeof(ctrl)));
  Document d2;
  EXPECT_NO_THROW(parse(d2, root2.dump()));
}

TEST(DataIntegrity, SetEscapesStringContent) {
  Document doc;
  Value root = parse(doc, R"({"k":"orig"})");
  root["k"].set(std::string_view("x\"y\\z\n"));
  EXPECT_EQ(root.dump(), R"({"k":"x\"y\\z\n"})");
  Document d2;
  EXPECT_NO_THROW(parse(d2, root.dump()));
}

// ── INTEGRITY: the relaxed parser accepts separator-less tokens, so the
// compact dump inserts separators and the output is LONGER than the source
// slice.  This previously overflowed the dump() / dump(buf) / dump_subtree_
// output buffers (heap-buffer-overflow, found by fuzz_parse under ASan).
TEST(DataIntegrity, RelaxedSeparatorlessDumpNoOverflow) {
  std::string inner(200, '-'); // 200 single-'-' number tokens, no separators
  std::string json = "[[" + inner + "]]";
  Document doc;
  Value r = parse(doc, json);
  // Root dump (no-arg): output must be valid and longer than the source.
  std::string d0 = r.dump();
  EXPECT_GT(d0.size(), json.size());
  // Root buffer-reuse dump(buf) variant must produce the same output.
  std::string buf;
  r.dump(buf);
  EXPECT_EQ(buf, d0);
  // Subtree dump (idx_ != 0) exercises dump_subtree_.
  std::string sub = r[0].dump();
  EXPECT_GT(sub.size(), inner.size());
  Document d2;
  EXPECT_NO_THROW(parse(d2, sub)); // dumped subtree is well-formed
}

// ── INTEGRITY: pretty-printing a relaxed/malformed tape must not blow up.
// This 449-byte fuzz artifact (found by fuzz_parse) previously expanded to a
// ~413 MB dump(2) output (≈1.26-million× amplification, ~1s CPU) because
// dump_pretty_ advanced via skip_value_ per element and re-traversed subtrees
// on the bracket-mismatched tape.  The linear single-pass rewrite bounds it.
TEST(DataIntegrity, PrettyPrintNoExponentialBlowup) {
  static const unsigned char kBytes[] = {
      91,91,91,49,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,49,44,0,50,125,44,110,117,108,108,91,44,93,44,50,91,52,93,49,
      44,50,125,44,91,44,0,49,44,110,117,108,108,91,44,93,44,49,91,44,
      52,50,93,50,125,44,91,44,0,49,125,91,50,44,91,91,91,49,0,48,
      0,0,0,0,50,91,52,93,49,44,50,125,44,91,44,0,49,125,91,50,
      44,110,117,108,108,91,44,93,44,50,91,52,93,49,44,50,125,44,91,50,
      44,52,49,44,1,125,12,91,50,44,52,50,44,0,0,0,0,0,93,91,
      49,44,0,50,125,44,110,117,108,108,91,44,93,44,50,91,52,93,49,44,
      50,125,44,91,44,0,49,125,91,50,44,110,117,108,108,91,44,93,44,50,
      91,52,93,49,44,50,125,44,91,34,34,58,0,0,0,16,25,0,0,0,
      0,0,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,
      14,14,14,14,14,14,14,14,0,49,44,52,49,44,1,0,50,91,52,93,
      49,44,50,125,44,91,44,0,49,125,91,50,44,110,117,108,108,91,44,93,
      44,50,91,52,44,0,49,125,91,50,44,110,117,108,108,91,44,93,44,50,
      91,52,93,49,44,50,125,44,91,50,44,52,49,44,1,125,12,91,50,44,
      52,50,44,0,0,0,0,0,93,91,49,44,0,50,125,44,110,117,108,108,
      91,44,93,44,50,91,52,93,49,44,50,125,44,91,44,0,49,125,91,50,
      44,110,117,108,108,91,44,93,44,50,91,52,93,49,44,50,125,44,91,110,
      117,108,108,91,44,93,44,50,52,91,93,49,34,34,58,0,0,0,0,25,
      0,0,0,0,0,14,14,14,14,14,14,14,14,0,44,50,125,44,91,44,
      52,93,49,44,52,49,44,1,93,125,12,91,50,44,52,50,44,52,49,44,
      1,125,12,91,7,44,52,93,93};
  std::string in(reinterpret_cast<const char *>(kBytes), sizeof(kBytes));
  Document doc;
  Value r = parse(doc, in); // relaxed parser accepts this byte soup
  const std::string compact = r.dump();
  const std::string pretty = r.dump(2);
  // Old behaviour: pretty ≈ 1.26e6× compact.  A generous linear bound still
  // catches any reintroduced super-linear blow-up.
  EXPECT_LT(pretty.size(), compact.size() * 4096u + 4096u);
}

// ── INTEGRITY: a JSON input ending in a dangling backslash inside a string
// value must not over-read.  from_json_direct<string>'s escape slow-path did
// "p += 2" on a trailing '\', pushing p past end, then out.assign() read out
// of bounds (found by fuzz_nexus + fuzz_direct under ASan).
TEST(DataIntegrity, NexusTrailingBackslashNoOverread) {
  const std::string a = std::string("{\"s\":\"abc") + '\\';      // ...abc\<EOF>
  const std::string b = std::string("{\"o\":\"x") + "\\\\\\";    // ...x\\\<EOF>
  EXPECT_NO_THROW({ auto r = qbuem::fuse<FuseStr>(a); (void)r; });
  EXPECT_NO_THROW({ auto r = qbuem::fuse<FuseStr>(b); (void)r; });
  // fuse_strict may legitimately throw on malformed input; we only require that
  // it does not read out of bounds (ASan/UBSan in CI enforce that).
  try { auto r = qbuem::fuse_strict<FuseStr>(a); (void)r; } catch (...) {}
  try { auto r = qbuem::fuse_strict<FuseStr>(b); (void)r; } catch (...) {}
}

// ── INTEGRITY: Nexus sequence decoding must make forward progress.  A
// non-numeric element where an int is expected previously left the cursor
// unadvanced, so the decode loop spun forever appending until OOM (a tiny
// input grew the vector to multiple GB).  Must terminate immediately.
TEST(DataIntegrity, NexusSequenceForwardProgress) {
  EXPECT_NO_THROW({ auto r = qbuem::fuse<FuseVec>(R"({"v":["x","y","z"]})"); (void)r; });
  EXPECT_NO_THROW({ auto r = qbuem::fuse<FuseVec>(R"({"v":[{},{}]})"); (void)r; });
  EXPECT_NO_THROW({ auto r = qbuem::fuse<FuseVec>(R"({"v":[1,"x",2]})"); (void)r; });
}

// ── INTEGRITY: inserting a value whose escaped form exceeds the 64KB token
// limit must not crash.  operator[] (noexcept) re-parses the inserted value
// via get_synthetic; that parse now rejects the oversize token, and the throw
// must not escape the noexcept accessor (previously std::terminate).
TEST(DataIntegrity, InsertOversizeEscapedValueNoTerminate) {
  Document doc;
  Value root = parse(doc, "{}");
  // 13000 control bytes -> each escapes to \u00XX (6 bytes) -> ~78KB token.
  std::string big(13000, '\x01');
  root.insert("k", std::string_view(big));
  Value v;
  EXPECT_NO_THROW({ v = root["k"]; }); // must not terminate
  EXPECT_FALSE(v.is_valid());          // unparseable synthetic -> invalid Value
}

// ── INTEGRITY: fuse_strict() must reject trailing content after the object,
// matching parse_strict()/rfc8259::validate().
TEST(DataIntegrity, FuseStrictRejectsTrailingContent) {
  EXPECT_THROW(fuse_strict<FuseRec>(R"({"a":1,"b":2}garbage)"),
               std::runtime_error);
  EXPECT_NO_THROW(fuse_strict<FuseRec>(R"({"a":1,"b":2}   )"));
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
