#include <qbuem_json/qbuem_json.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <cmath>
#include <cstdint>

// A minimized (ASCII-sanitized) heterogeneous-sibling-objects document derived
// from twitter.json, used by the KeyLenCache regression test below.
#include "keycache_regression.inc"

using namespace qbuem;

// ── Compile-time UAF-guard regression ───────────────────────────────────────
// Every zero-copy parse entry point returns a Value that views into its input
// buffer.  Passing a *temporary* std::string would dangle, so each overload has
// a deleted `const std::string&&` companion that turns the misuse into a compile
// error.  These static_asserts lock that contract in: an lvalue (kept alive) is
// invocable, but a prvalue std::string is NOT.  A requires-expression that would
// select a deleted overload is ill-formed, hence unsatisfied — so the negative
// asserts succeed exactly when the guards are present.  If anyone drops a guard,
// this file stops compiling.
// NOTE: every detector is parameterized on the document type `Doc` so the call
// lives in a *dependent* context.  There, selecting a deleted overload is a soft
// (SFINAE) failure that makes the requires-expression evaluate to false; written
// inline with a concrete type it would instead be a hard "call to deleted
// function" error.  So the templates below are load-bearing, not stylistic.
namespace {
template <class Doc>
inline constexpr bool lvalue_ok =
    requires(Doc &d, std::string &s) { qbuem::json::parse_reuse(d, s); };
template <class Doc>
inline constexpr bool reuse_rvalue_rejected =
    !requires(Doc &d) { qbuem::json::parse_reuse(d, std::string{"{}"}); };
template <class Doc>
inline constexpr bool partial_rvalue_rejected =
    !requires(Doc &d) { qbuem::json::parse_partial(d, std::string{"{}"}); };
template <class Doc>
inline constexpr bool strict_dv_rvalue_rejected =
    !requires(Doc &d) { qbuem::json::rfc8259::parse_strict(d, std::string{"{}"}); };
template <class Doc>
inline constexpr bool parse_rvalue_rejected =
    !requires(Doc &d) { qbuem::parse(d, std::string{"{}"}); };
template <class Doc>
inline constexpr bool parse_strict_rvalue_rejected =
    !requires(Doc &d) { qbuem::parse_strict(d, std::string{"{}"}); };

// Positive: an lvalue (kept alive) is invocable on both document handles.
static_assert(lvalue_ok<qbuem::Document>, "Document& lvalue parse must compile");
static_assert(lvalue_ok<qbuem::json::DocumentView>,
              "DocumentView& lvalue parse must compile");
// Negative: a prvalue std::string selects the deleted UAF guard on every
// zero-copy entry point (Document& family + DocumentView& family).
static_assert(reuse_rvalue_rejected<qbuem::Document>,
              "parse_reuse(Document&, std::string&&) must be deleted (UAF)");
static_assert(reuse_rvalue_rejected<qbuem::json::DocumentView>,
              "parse_reuse(DocumentView&, std::string&&) must be deleted (UAF)");
static_assert(partial_rvalue_rejected<qbuem::json::DocumentView>,
              "parse_partial(DocumentView&, std::string&&) must be deleted (UAF)");
static_assert(strict_dv_rvalue_rejected<qbuem::json::DocumentView>,
              "parse_strict(DocumentView&, std::string&&) must be deleted (UAF)");
static_assert(parse_rvalue_rejected<qbuem::Document>,
              "parse(Document&, std::string&&) must be deleted (UAF)");
static_assert(parse_strict_rvalue_rejected<qbuem::Document>,
              "parse_strict(Document&, std::string&&) must be deleted (UAF)");
} // namespace

// Struct used by the Nexus (fuse) data-integrity tests below.
struct FuseRec { int a; long b; };
QBUEM_JSON_FIELDS(FuseRec, a, b)

// Struct with string fields to exercise the Nexus from_json_direct<string> path.
struct FuseStr { std::string s; std::optional<std::string> o; };
QBUEM_JSON_FIELDS(FuseStr, s, o)

// Struct with a vector to exercise the Nexus sequence-decoding loop.
struct FuseVec { std::vector<int> v; };
QBUEM_JSON_FIELDS(FuseVec, v)

// Struct with an unsigned-64 field for serialize round-trip tests.
struct U64Rec { uint64_t x; };
QBUEM_JSON_FIELDS(U64Rec, x)

// Structs for the Nexus security regression tests.
struct AdminRec { long is_admin_lvl; std::string owner; };
QBUEM_JSON_FIELDS(AdminRec, is_admin_lvl, owner)
struct TreeNode { long v; std::vector<TreeNode> kids; };
QBUEM_JSON_FIELDS(TreeNode, v, kids)
struct AbcRec { long a; std::string b; std::string c; };
QBUEM_JSON_FIELDS(AbcRec, a, b, c)
// For the duplicate-key determinism test.
struct DupRec { long role; long x; };
QBUEM_JSON_FIELDS(DupRec, role, x)

// Performance regression guard: NexusScanner::fill() has a fast path that
// computes the key hash *during* the key scan (read_key_h) and dispatches via
// nexus_pulse_h — gated on the HasNexusPulseH concept.  If that concept's
// requires-clause ever drifts out of sync with nexus_pulse_h's real arity, it
// silently evaluates false and every struct decode falls back to a slow
// double-scan (read_key + a separate fast_key_hash) — a ~15% fuse<T> regression
// that is functionally correct, so no runtime test catches it.  Assert the fast
// path stays enabled for a normal QBUEM_JSON_FIELDS struct.
static_assert(qbuem::json::detail::HasNexusPulseH<AbcRec>,
              "Nexus fast hash-dispatch path is disabled — HasNexusPulseH must "
              "track nexus_pulse_h's signature (else ~15% fuse<T> regression)");

// ── RFC 8259 §8.1 strict UTF-8 well-formedness ──────────────────────────────
// parse_strict() must reject malformed UTF-8 byte sequences in string content
// (overlong encodings, UTF-8-encoded surrogates, > U+10FFFF, lone continuation
// bytes, truncated sequences).  The relaxed parser stays byte-transparent.
namespace {
bool strict_accepts(const std::string &j) {
  qbuem::json::DocumentView d;
  try { (void)qbuem::json::rfc8259::parse_strict(d, j); return true; }
  catch (...) { return false; }
}
bool relaxed_accepts(const std::string &j) {
  qbuem::Document d;
  try { auto v = qbuem::parse(d, j); return v.is_string() || v.is_array(); }
  catch (...) { return false; }
}
} // namespace

TEST(Rfc8259Utf8, AcceptsValidMultibyte) {
  EXPECT_TRUE(strict_accepts("\"\xC3\xA9\""));          // U+00E9 é (2-byte)
  EXPECT_TRUE(strict_accepts("\"\xE2\x9C\x93\""));      // U+2713 ✓ (3-byte)
  EXPECT_TRUE(strict_accepts("\"\xF0\x9F\x98\x80\""));  // U+1F600 😀 (4-byte)
  EXPECT_TRUE(strict_accepts("\"\xF4\x8F\xBF\xBF\""));  // U+10FFFF (max)
  EXPECT_TRUE(strict_accepts("\"plain ascii\""));
}

TEST(Rfc8259Utf8, RejectsMalformedBytes) {
  EXPECT_FALSE(strict_accepts("\"\xC3\""));             // truncated 2-byte lead
  EXPECT_FALSE(strict_accepts("\"\x80\""));             // lone continuation
  EXPECT_FALSE(strict_accepts("\"\xC0\xAF\""));         // overlong '/' (security)
  EXPECT_FALSE(strict_accepts("\"\xE0\x80\xAF\""));     // overlong 3-byte
  EXPECT_FALSE(strict_accepts("\"\xED\xA0\x80\""));     // U+D800 surrogate
  EXPECT_FALSE(strict_accepts("\"\xED\xBF\xBF\""));     // U+DFFF surrogate
  EXPECT_FALSE(strict_accepts("\"\xF4\x90\x80\x80\"")); // > U+10FFFF
  EXPECT_FALSE(strict_accepts("\"\xF5\x80\x80\x80\"")); // invalid lead 0xF5
  EXPECT_FALSE(strict_accepts("\"\xE2\x9C\""));         // truncated 3-byte
  EXPECT_FALSE(strict_accepts("\"\xC3\x28\""));         // bad continuation
}

TEST(Rfc8259Utf8, RelaxedStaysByteTransparent) {
  // The relaxed (zero-copy) parser does NOT validate UTF-8 — it accepts the raw
  // bytes so as<string>() can return a non-owning slice.  Documented behavior.
  EXPECT_TRUE(relaxed_accepts("\"\xC0\xAF\""));
  EXPECT_TRUE(relaxed_accepts("\"\xED\xA0\x80\""));
}

// ── Typed parse errors with byte offset (F3) ────────────────────────────────
TEST(ParseError, CarriesByteOffset) {
  qbuem::Document d;
  std::string bad = "[1,2,";        // truncated — fails at end of input
  try {
    auto v = qbuem::parse(d, bad);
    (void)v;
    FAIL() << "expected parse_error";
  } catch (const qbuem::parse_error &e) {
    EXPECT_EQ(e.offset(), bad.size()); // ran out of input at offset 5
    EXPECT_NE(std::string(e.what()).find("offset"), std::string::npos);
  }
}

TEST(ParseError, OffsetPointsAtTrailingGarbage) {
  qbuem::Document d;
  std::string g = "[1,2]xx";
  try { auto v = qbuem::parse(d, g); (void)v; FAIL() << "expected throw"; }
  catch (const qbuem::parse_error &e) { EXPECT_EQ(e.offset(), 5u); }
}

TEST(ParseError, BackwardCompatibleCatch) {
  // parse_error derives from std::runtime_error: existing catch blocks still work.
  qbuem::Document d;
  std::string bad = "nul";
  bool caught_runtime = false, caught_exception = false;
  try { auto v = qbuem::parse(d, bad); (void)v; } catch (const std::runtime_error &) {
    caught_runtime = true;
  }
  try { auto v = qbuem::parse(d, bad); (void)v; } catch (const std::exception &) {
    caught_exception = true;
  }
  EXPECT_TRUE(caught_runtime);
  EXPECT_TRUE(caught_exception);
}

TEST(ParseError, StrictCarriesOffsetAndReason) {
  qbuem::json::DocumentView d;
  std::string lz = "{\"a\":01}"; // leading zero — strict violation
  try {
    (void)qbuem::json::rfc8259::parse_strict(d, lz);
    FAIL() << "expected parse_error";
  } catch (const qbuem::parse_error &e) {
    EXPECT_EQ(e.offset(), 5u);
    EXPECT_NE(std::string(e.what()).find("leading zero"), std::string::npos);
  }
}

// ── Duplicate-key determinism (RFC 8259 §4: SHOULD, not MUST, be unique) ─────
// Duplicates are valid JSON, so both parsers accept them; the documented,
// locked-in resolution is DOM first-wins / Nexus last-wins.  This test guards
// against an accidental change to either (a silent parser-differential).
TEST(DuplicateKeys, DomIsFirstWins) {
  qbuem::Document d;
  std::string j = "{\"role\":1,\"x\":9,\"role\":2}";
  auto v = qbuem::parse(d, j);
  EXPECT_EQ(v["role"].as<int64_t>(), 1); // first-wins
  EXPECT_EQ(v["x"].as<int64_t>(), 9);
}

TEST(DuplicateKeys, NexusIsLastWins) {
  std::string j = "{\"role\":1,\"x\":9,\"role\":2}";
  auto r = qbuem::fuse<DupRec>(j);
  EXPECT_EQ(r.role, 2); // last-wins (matches JS/Python/Go ecosystem)
  EXPECT_EQ(r.x, 9);
}

TEST(DuplicateKeys, StrictAcceptsThemAsValidJson) {
  // RFC 8259 conformance: duplicates are valid JSON, so the strict validator
  // (rfc8259::validate) must NOT reject them — JSONTestSuite y_object_duplicated_key.
  qbuem::json::DocumentView d;
  std::string j = "{\"a\":1,\"a\":2}";
  EXPECT_NO_THROW((void)qbuem::json::rfc8259::parse_strict(d, j));
}

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
  const std::string out2 = root2.dump(); // hold: parse() borrows the buffer
  Document d2;
  EXPECT_NO_THROW(parse(d2, out2));
}

TEST(DataIntegrity, SetEscapesStringContent) {
  Document doc;
  Value root = parse(doc, R"({"k":"orig"})");
  root["k"].set(std::string_view("x\"y\\z\n"));
  EXPECT_EQ(root.dump(), R"({"k":"x\"y\\z\n"})");
  const std::string out = root.dump(); // hold: parse() borrows the buffer
  Document d2;
  EXPECT_NO_THROW(parse(d2, out));
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

// ── INTEGRITY: the parser must not mis-parse valid heterogeneous JSON.
// The removed KeyLenCache fast path predicted an object's key lengths from a
// prior sibling object at the same depth and validated the guess with
// `s[cl]=='"' && s[cl+1]==':'`.  When a real key was shorter than the cached
// length, that pattern could match coincidentally inside the value, mis-detect
// the key boundary, and make qbuem REJECT valid JSON (it could not even
// re-parse its own compact dump of twitter.json).  This minimized fragment
// reproduced it; it must now parse, dump, and re-parse cleanly.
TEST(DataIntegrity, HeterogeneousSiblingObjectsParse) {
  std::string in = kKeyCacheRegressionJson;
  Document doc;
  Value r;
  ASSERT_NO_THROW({ r = parse(doc, in); });
  ASSERT_TRUE(r.is_object());
  std::string s1 = r.dump();
  Document d2;
  EXPECT_NO_THROW({ parse(d2, s1); }); // re-parse our own compact dump
}

// ── INTEGRITY: number literals >= 64 chars must not be truncated.  parse_f64's
// strtod fallback used a fixed char buf[64], truncating long literals to 63
// chars and corrupting the magnitude (1e64 parsed as 1e62).
TEST(DataIntegrity, LongNumberLiteralNotTruncated) {
  Document doc;
  std::string j = "[1" + std::string(64, '0') + "]"; // 1e64 as a 65-char literal
  Value r = parse(doc, j);
  EXPECT_DOUBLE_EQ(r[0].as<double>(), 1e64);
  Document d2;
  std::string j2 = "[9" + std::string(80, '0') + "]"; // 9e80
  Value r2 = parse(d2, j2);
  EXPECT_DOUBLE_EQ(r2[0].as<double>(), 9e80);
}

// ── INTEGRITY: fuse_strict() must reject trailing content after the object,
// matching parse_strict()/rfc8259::validate().
TEST(DataIntegrity, FuseStrictRejectsTrailingContent) {
  EXPECT_THROW(fuse_strict<FuseRec>(R"({"a":1,"b":2}garbage)"),
               std::runtime_error);
  EXPECT_NO_THROW(fuse_strict<FuseRec>(R"({"a":1,"b":2}   )"));
}

// ── TYPE SAFETY: as<narrow-integer> must range-check, not silently truncate.
// Previously as<uint8_t>(256) returned 0 and as<uint8_t>(-1) returned 255
// (static_cast wrap) — a silent data-corruption hazard.
TEST(TypeSafety, NarrowIntegerOverflowThrows) {
  Document doc;
  Value r = parse(doc, "[256, -1, 1000, 127, 255, 32768, -129]");
  EXPECT_EQ(r[3].as<int8_t>(), 127);                     // in range
  EXPECT_EQ(r[4].as<uint8_t>(), 255);                    // in range
  EXPECT_EQ(r[4].as<uint16_t>(), 255u);                  // widening fine
  EXPECT_THROW(r[0].as<int8_t>(), std::runtime_error);   // 256 > int8 max
  EXPECT_THROW(r[0].as<uint8_t>(), std::runtime_error);  // 256 > uint8 max
  EXPECT_THROW(r[2].as<uint8_t>(), std::runtime_error);  // 1000 > uint8 max
  EXPECT_THROW(r[1].as<uint8_t>(), std::runtime_error);  // -1 into unsigned
  EXPECT_THROW(r[1].as<uint32_t>(), std::runtime_error); // -1 into unsigned
  EXPECT_THROW(r[5].as<int16_t>(), std::runtime_error);  // 32768 > int16 max
  EXPECT_THROW(r[6].as<int8_t>(), std::runtime_error);   // -129 < int8 min
}

// ── TYPE SAFETY: the full uint64 range must be readable; cross-sign overflow
// must throw.  Previously as<uint64_t> parsed via int64_t and could not read
// values above INT64_MAX (it threw on UINT64_MAX).
TEST(TypeSafety, FullUint64Range) {
  Document doc;
  Value r = parse(doc, "[18446744073709551615, 9223372036854775808, "
                       "9223372036854775807]");
  EXPECT_EQ(r[0].as<uint64_t>(), 18446744073709551615ULL); // UINT64_MAX
  EXPECT_EQ(r[1].as<uint64_t>(), 9223372036854775808ULL);  // INT64_MAX + 1
  EXPECT_EQ(r[2].as<int64_t>(), 9223372036854775807LL);    // INT64_MAX
  EXPECT_THROW(r[0].as<int64_t>(), std::runtime_error);    // UINT64_MAX > int64
  EXPECT_THROW(r[1].as<int64_t>(), std::runtime_error);    // INT64_MAX+1 > int64
}

// ── TYPE SAFETY: float extremes (overflow -> inf, underflow -> 0, signed zero).
TEST(TypeSafety, FloatExtremes) {
  Document doc;
  Value r = parse(doc, "[1e400, 1e-400, -0, 5e-324, -1e-400]");
  EXPECT_TRUE(std::isinf(r[0].as<double>()));            // overflow -> +inf
  EXPECT_EQ(r[1].as<double>(), 0.0);                     // underflow -> 0
  EXPECT_TRUE(std::signbit(r[2].as<double>()));          // -0 sign preserved
  EXPECT_GT(r[3].as<double>(), 0.0);                     // smallest subnormal
  EXPECT_TRUE(std::signbit(r[4].as<double>()));          // -0 from neg underflow
}

// ── TYPE SAFETY: serializing an unsigned value above INT64_MAX must not emit
// "-1".  Value::set/insert/scalar_to_json_ cast to int64_t; fixed to dispatch
// on signedness like the struct writer already did.
TEST(TypeSafety, SerializeUnsigned64RoundTrips) {
  const uint64_t umax = 18446744073709551615ULL;
  Document doc;
  Value r = parse(doc, "{}");
  r.insert("u", umax);
  const std::string dumped = r.dump(); // hold: parse() keeps a view into this
  EXPECT_NE(dumped.find("18446744073709551615"), std::string::npos);
  Document d2;
  Value r2 = parse(d2, dumped);
  EXPECT_EQ(r2["u"].as<uint64_t>(), umax);

  // struct write path (was already correct — guard against regression)
  U64Rec s{umax};
  std::string j = qbuem::write(s);
  EXPECT_NE(j.find("18446744073709551615"), std::string::npos);
  EXPECT_EQ(qbuem::read<U64Rec>(j).x, umax);
}

// ── SECURITY: dump_changes_ used a fixed Frame stk[64] while the parser allows
// nesting to 1088; a valid deep doc + any mutation + dump() smashed the stack.
TEST(SecurityHardening, DumpChangesDeepNestNoStackSmash) {
  std::string j(300, '[');
  j += "1";
  j.append(300, ']');
  Document doc;
  Value r = parse(doc, j);
  Value cur = r;
  for (int i = 0; i < 299; ++i)
    cur = cur[size_t(0)];
  cur.push_back(9); // route dump() to dump_changes_
  EXPECT_NO_THROW(r.dump());
}

// ── SECURITY: Nexus dispatch is by key hash; the hash is collision-prone, so a
// colliding untrusted key must NOT be routed into a field it doesn't name.
TEST(SecurityHardening, NexusHashCollisionNoFieldSpoof) {
  // "iS_adMin_Lvl" collides with "is_admin_lvl" under fast_key_hash.
  auto r = qbuem::fuse<AdminRec>(R"({"owner":"bob","iS_adMin_Lvl":99})");
  EXPECT_EQ(r.is_admin_lvl, 0); // spoof rejected (not 99)
  auto r2 = qbuem::fuse<AdminRec>(R"({"is_admin_lvl":7,"owner":"x"})");
  EXPECT_EQ(r2.is_admin_lvl, 7); // legitimate key still works
}

// ── SECURITY: Nexus typed recursion (recursive struct) must be depth-bounded,
// not crash the stack on deeply-nested untrusted input.
TEST(SecurityHardening, NexusDeepRecursionRejected) {
  const int D = 30000;
  std::string j;
  for (int i = 0; i < D; ++i)
    j += "{\"kids\":[";
  j += "{\"v\":1}";
  for (int i = 0; i < D; ++i)
    j += "]}";
  EXPECT_THROW(qbuem::fuse<TreeNode>(j), std::runtime_error);
}

// ── SECURITY/INTEGRITY: a type-mismatched value (string where number expected)
// must not desync the cursor and silently drop the rest of the object.
TEST(SecurityHardening, NexusCursorNoFieldDrop) {
  auto r = qbuem::fuse<AbcRec>(R"({"a":"X","b":"BEE","c":"CEE"})");
  EXPECT_EQ(r.b, "BEE");
  EXPECT_EQ(r.c, "CEE");
}

// ── API: decoded() returns the unescaped logical string; as<string> stays raw.
TEST(ApiUsability, DecodedUnescapesString) {
  Document doc;
  Value r = parse(doc, R"({"s":"a\nb\t\"\\\/Aé"})");
  // expected: a <nl> b <tab> " \ / A é(0xC3 0xA9)
  std::string expected = "a";
  expected += '\n'; expected += 'b'; expected += '\t';
  expected += '"'; expected += '\\'; expected += '/'; expected += 'A';
  expected += static_cast<char>(0xC3); expected += static_cast<char>(0xA9);
  EXPECT_EQ(r["s"].decoded(), expected);
  // as<string_view> remains the RAW on-the-wire slice
  EXPECT_EQ(std::string(r["s"].as<std::string_view>()).find("\\n"), 1u);

  // surrogate pair 😀 -> U+1F600 (😀 = F0 9F 98 80)
  Value r2 = parse(doc, R"({"e":"😀"})");
  std::string emoji;
  emoji += static_cast<char>(0xF0); emoji += static_cast<char>(0x9F);
  emoji += static_cast<char>(0x98); emoji += static_cast<char>(0x80);
  EXPECT_EQ(r2["e"].decoded(), emoji);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
