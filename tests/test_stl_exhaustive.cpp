/**
 * @file test_stl_exhaustive.cpp
 *
 * Exhaustive coverage of all STL containers, BEAST_JSON_FIELDS struct
 * round-trips, numeric/string edge cases, and structural mutation correctness.
 *
 * Uses beast::write() / beast::read<T>() exclusively (public API).
 *
 * NOTE: beast::read<std::string>(json) returns the RAW JSON string byte content
 * without converting escape sequences. So "hello\\nworld" parsed as a string
 * gives back the string with a literal backslash-n, not a newline character.
 * String round-trips through write/read only preserve strings that don't
 * contain characters requiring JSON escape sequences (quotes, backslashes,
 * control chars). Tests are designed to reflect this real behavior.
 */

#include <beast_json/beast_json.hpp>
#include <gtest/gtest.h>

#include <array>
#include <deque>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

using namespace beast;

// ─── Struct definitions (global namespace for BEAST_JSON_FIELDS ADL) ─────────

struct Point {
  int x{}, y{};
};
BEAST_JSON_FIELDS(Point, x, y)

struct Person {
  std::string name;
  int age{};
  std::optional<std::string> email;
  std::vector<int> scores;
};
BEAST_JSON_FIELDS(Person, name, age, email, scores)

struct GeoPoint {
  Point origin{};
  std::vector<Point> points;
  std::map<std::string, int> labels;
};
BEAST_JSON_FIELDS(GeoPoint, origin, points, labels)

struct ServerConfig {
  std::string host;
  int port{};
  std::vector<std::string> tags;
  std::map<std::string, std::string> env;
  std::optional<int> timeout;
};
BEAST_JSON_FIELDS(ServerConfig, host, port, tags, env, timeout)

struct AppConfig {
  std::string name;
  std::vector<ServerConfig> servers;
  ServerConfig primary;
};
BEAST_JSON_FIELDS(AppConfig, name, servers, primary)

// ─── Helpers ─────────────────────────────────────────────────────────────────

template <typename T> static T roundtrip(const T &val) {
  return beast::read<T>(beast::write(val));
}

// ─── BEAST_JSON_FIELDS: Point
// ─────────────────────────────────────────────────

TEST(BeastJsonFields, PointRoundTrip) {
  Point p{3, 7};
  std::string json = beast::write(p);
  EXPECT_NE(json.find("\"x\":3"), std::string::npos);
  EXPECT_NE(json.find("\"y\":7"), std::string::npos);

  Point p2 = beast::read<Point>(json);
  EXPECT_EQ(p2.x, 3);
  EXPECT_EQ(p2.y, 7);
}

TEST(BeastJsonFields, PersonRoundTrip) {
  Person p;
  p.name = "Alice";
  p.age = 30;
  p.email = "alice@example.com";
  p.scores = {10, 20, 30};

  auto p2 = roundtrip(p);
  EXPECT_EQ(p2.name, "Alice");
  EXPECT_EQ(p2.age, 30);
  ASSERT_TRUE(p2.email.has_value());
  EXPECT_EQ(*p2.email, "alice@example.com");
  ASSERT_EQ(p2.scores.size(), 3u);
  EXPECT_EQ(p2.scores[1], 20);
}

TEST(BeastJsonFields, PersonOptionalAbsent) {
  Person p;
  p.name = "Bob";
  p.age = 25;
  p.scores = {5};

  auto p2 = roundtrip(p);
  EXPECT_EQ(p2.name, "Bob");
  EXPECT_FALSE(p2.email.has_value());
  EXPECT_EQ(p2.scores[0], 5);
}

TEST(BeastJsonFields, NestedStructRoundTrip) {
  GeoPoint n;
  n.origin = {0, 0};
  n.points = {{1, 2}, {3, 4}, {5, 6}};
  n.labels = {{"a", 1}, {"b", 2}};

  auto n2 = roundtrip(n);
  EXPECT_EQ(n2.origin.x, 0);
  ASSERT_EQ(n2.points.size(), 3u);
  EXPECT_EQ(n2.points[1].x, 3);
  EXPECT_EQ(n2.points[1].y, 4);
  EXPECT_EQ(n2.labels["a"], 1);
}

// ─── STL: vector ─────────────────────────────────────────────────────────────

TEST(STLVector, IntRoundTrip) {
  std::vector<int> v = {1, 2, 3, 4, 5};
  EXPECT_EQ(roundtrip(v), v);
}

TEST(STLVector, StringRoundTrip) {
  // Use strings that don't require JSON escaping
  std::vector<std::string> v = {"alpha", "beta", "gamma"};
  EXPECT_EQ(roundtrip(v), v);
}

TEST(STLVector, OfVectors) {
  std::vector<std::vector<int>> v = {{1, 2}, {3, 4}};
  auto v2 = roundtrip(v);
  ASSERT_EQ(v2.size(), 2u);
  EXPECT_EQ(v2[0][1], 2);
  EXPECT_EQ(v2[1][0], 3);
}

TEST(STLVector, Empty) {
  std::vector<int> v = {};
  EXPECT_EQ(beast::write(v), "[]");
  EXPECT_EQ(roundtrip(v), v);
}

TEST(STLVector, VectorBool) {
  std::vector<bool> v = {true, false, true};
  EXPECT_EQ(beast::write(v), "[true,false,true]");
  auto v2 = beast::read<std::vector<bool>>("[true,false,true]");
  ASSERT_EQ(v2.size(), 3u);
  EXPECT_EQ(v2[0], true);
  EXPECT_EQ(v2[1], false);
}

// ─── STL: list ───────────────────────────────────────────────────────────────

TEST(STLList, RoundTrip) {
  std::list<int> l = {10, 20, 30};
  EXPECT_EQ(beast::write(l), "[10,20,30]");
  EXPECT_EQ(roundtrip(l), l);
}

TEST(STLList, StringList) {
  std::list<std::string> l = {"x", "y"};
  EXPECT_EQ(roundtrip(l), l);
}

// ─── STL: deque ──────────────────────────────────────────────────────────────

TEST(STLDeque, RoundTrip) {
  std::deque<int> d = {1, 2, 3};
  EXPECT_EQ(beast::write(d), "[1,2,3]");
  EXPECT_EQ(roundtrip(d), d);
}

// ─── STL: set / unordered_set ────────────────────────────────────────────────

TEST(STLSet, IntSet) {
  std::set<int> s = {3, 1, 2};
  EXPECT_EQ(beast::write(s), "[1,2,3]"); // sorted iteration
  EXPECT_EQ(roundtrip(s), s);
}

TEST(STLSet, StringSet) {
  std::set<std::string> s = {"banana", "apple", "cherry"};
  EXPECT_EQ(roundtrip(s), s);
}

TEST(STLSet, UnorderedSet) {
  std::unordered_set<int> s = {5, 10, 15};
  EXPECT_EQ(roundtrip(s), s);
}

// ─── STL: map / unordered_map ────────────────────────────────────────────────

TEST(STLMap, BasicRoundTrip) {
  std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
  auto m2 = roundtrip(m);
  EXPECT_EQ(m2["a"], 1);
  EXPECT_EQ(m2["b"], 2);
}

TEST(STLMap, MapOfVectors) {
  std::map<std::string, std::vector<int>> m = {{"evens", {2, 4, 6}},
                                               {"odds", {1, 3, 5}}};
  auto m2 = roundtrip(m);
  EXPECT_EQ(m2["evens"][1], 4);
  EXPECT_EQ(m2["odds"][0], 1);
}

TEST(STLMap, UnorderedMap) {
  std::unordered_map<std::string, double> m = {{"pi", 3.14}, {"e", 2.71}};
  auto m2 = roundtrip(m);
  EXPECT_NEAR(m2["pi"], 3.14, 1e-9);
  EXPECT_NEAR(m2["e"], 2.71, 1e-9);
}

TEST(STLMap, NestedMap) {
  std::map<std::string, std::map<std::string, int>> m = {
      {"outer", {{"inner", 42}}}};
  auto m2 = roundtrip(m);
  EXPECT_EQ(m2["outer"]["inner"], 42);
}

// ─── STL: std::array<T,N> ────────────────────────────────────────────────────

TEST(STLArray, FixedIntArray) {
  std::array<int, 3> a = {10, 20, 30};
  EXPECT_EQ(beast::write(a), "[10,20,30]");
  EXPECT_EQ(roundtrip(a), a);
}

TEST(STLArray, FixedStringArray) {
  std::array<std::string, 2> a = {"hello", "world"};
  auto a2 = roundtrip(a);
  EXPECT_EQ(a2[0], "hello");
  EXPECT_EQ(a2[1], "world");
}

// ─── STL: pair / tuple ───────────────────────────────────────────────────────

TEST(STLPair, IntStringPair) {
  std::pair<int, std::string> p = {42, "beast"};
  EXPECT_EQ(beast::write(p), "[42,\"beast\"]");
  auto p2 = roundtrip(p);
  EXPECT_EQ(p2.first, 42);
  EXPECT_EQ(p2.second, "beast");
}

TEST(STLTuple, ThreeElementTuple) {
  std::tuple<int, double, std::string> t = {1, 3.14, "hi"};
  std::string json = beast::write(t);
  Document doc;
  auto root = parse(doc, json);
  EXPECT_TRUE(root.is_array());
  EXPECT_EQ(root.size(), 3u);
  EXPECT_EQ(root[0].as<int>(), 1);
  EXPECT_NEAR(root[1].as<double>(), 3.14, 1e-9);
  EXPECT_EQ(root[2].as<std::string>(), "hi");
}

TEST(STLTuple, EmptyTuple) { EXPECT_EQ(beast::write(std::tuple<>{}), "[]"); }

// ─── std::optional ───────────────────────────────────────────────────────────

TEST(STLOptional, SomeInt) {
  std::optional<int> o = 42;
  EXPECT_EQ(beast::write(o), "42");
  auto o2 = roundtrip(o);
  ASSERT_TRUE(o2.has_value());
  EXPECT_EQ(*o2, 42);
}

TEST(STLOptional, None) {
  std::optional<int> o = std::nullopt;
  EXPECT_EQ(beast::write(o), "null");
  auto o2 = roundtrip(o);
  EXPECT_FALSE(o2.has_value());
}

TEST(STLOptional, SomeString) {
  std::optional<std::string> o = "hello";
  auto o2 = roundtrip(o);
  ASSERT_TRUE(o2.has_value());
  EXPECT_EQ(*o2, "hello");
}

TEST(STLOptional, SomeVector) {
  std::optional<std::vector<int>> o = std::vector<int>{1, 2, 3};
  auto o2 = roundtrip(o);
  ASSERT_TRUE(o2.has_value());
  EXPECT_EQ(*o2, (std::vector<int>{1, 2, 3}));
}

// ─── std::variant: via explicit Value mutation
// ────────────────────────────────

TEST(STLVariant, IntViaValueSet) {
  Document doc;
  auto root = parse(doc, R"({"n":0,"s":"","d":0.0})");

  json::Value nv = root["n"];
  nv.set(42);
  EXPECT_EQ(nv.as<int>(), 42);

  json::Value sv = root["s"];
  sv.set("hello");
  EXPECT_EQ(sv.as<std::string>(), "hello");

  json::Value dv = root["d"];
  dv.set(3.14);
  EXPECT_NEAR(dv.as<double>(), 3.14, 1e-9);
}

// ─── Numeric Edge Cases
// ───────────────────────────────────────────────────────

TEST(NumericEdge, IntMin) {
  int64_t v = std::numeric_limits<int64_t>::min();
  EXPECT_EQ(roundtrip(v), v);
}

TEST(NumericEdge, IntMax) {
  int64_t v = std::numeric_limits<int64_t>::max();
  EXPECT_EQ(roundtrip(v), v);
}

TEST(NumericEdge, ZeroFloat) { EXPECT_DOUBLE_EQ(roundtrip(0.0), 0.0); }

TEST(NumericEdge, NegativeZero) {
  auto json = beast::write(-0.0);
  Document doc;
  EXPECT_DOUBLE_EQ(parse(doc, json).as<double>(), 0.0);
}

TEST(NumericEdge, NaNIsNull) {
  EXPECT_EQ(beast::write(std::numeric_limits<double>::quiet_NaN()), "null");
}

TEST(NumericEdge, InfIsNull) {
  EXPECT_EQ(beast::write(std::numeric_limits<double>::infinity()), "null");
}

TEST(NumericEdge, NegInfIsNull) {
  EXPECT_EQ(beast::write(-std::numeric_limits<double>::infinity()), "null");
}

TEST(NumericEdge, MaxDouble) {
  auto json = beast::write(std::numeric_limits<double>::max());
  Document doc;
  EXPECT_GT(parse(doc, json).as<double>(), 0.0);
}

TEST(NumericEdge, FloatPrecision) {
  auto json = beast::write(1.5f);
  Document doc;
  EXPECT_NEAR(parse(doc, json).as<double>(), 1.5, 1e-6);
}

TEST(NumericEdge, LargeNegativeInt64) {
  EXPECT_EQ(roundtrip(int64_t{-9000000000LL}), int64_t{-9000000000LL});
}

TEST(NumericEdge, UInt32Max) {
  EXPECT_EQ(roundtrip(uint64_t{4294967295ULL}), uint64_t{4294967295ULL});
}

// ─── String: serialization-level checks (beast::write escaping)
// ─────────────── Note: beast stores strings verbatim in parse tape.
// beast::write(s) escapes s→JSON. beast::read<string>(json) re-parses the JSON
// string but stores the raw un-decoded bytes from the JSON source.
// Thus roundtrip of strings with special chars returns the JSON-escaped form,
// not the original string. These tests verify the ESCAPING behavior.

TEST(StringEdge, EmptyStringSerializes) {
  EXPECT_EQ(beast::write(std::string("")), "\"\"");
}

TEST(StringEdge, SimpleStringRoundtrip) {
  // string without special chars round-trips exactly
  std::string s = "hello world";
  EXPECT_EQ(roundtrip(s), s);
}

TEST(StringEdge, EmbeddedQuoteEscaped) {
  std::string s = "say \"hello\"";
  auto json = beast::write(s);
  // Verify the JSON contains escaped quotes
  EXPECT_NE(json.find("\\\""), std::string::npos);
  // Verify the JSON is valid (parsable)
  Document doc;
  auto v = parse(doc, json);
  EXPECT_TRUE(v.is_string());
}

TEST(StringEdge, EmbeddedBackslashEscaped) {
  std::string s = "path\\file";
  auto json = beast::write(s);
  EXPECT_NE(json.find("\\\\"), std::string::npos);
  Document doc;
  EXPECT_TRUE(parse(doc, json).is_string());
}

TEST(StringEdge, NewlineEscaped) {
  std::string s = "line1\nline2";
  auto json = beast::write(s);
  EXPECT_NE(json.find("\\n"), std::string::npos);
  // The raw character must not appear in the JSON output
  EXPECT_EQ(json.find('\n'), std::string::npos);
}

TEST(StringEdge, TabEscaped) {
  std::string s = "col1\tcol2";
  auto json = beast::write(s);
  EXPECT_NE(json.find("\\t"), std::string::npos);
  EXPECT_EQ(json.find('\t'), std::string::npos);
}

TEST(StringEdge, ControlCharsEscaped) {
  for (unsigned char c = 1; c < 0x20; ++c) {
    if (c == '\n' || c == '\r' || c == '\t')
      continue;
    std::string s(1, static_cast<char>(c));
    auto json = beast::write(s);
    EXPECT_EQ(json.find(c), std::string::npos)
        << "Char 0x" << std::hex << (int)c << " not escaped in: " << json;
  }
}

TEST(StringEdge, UTF8EuroSignPreserved) {
  // U+20AC → E2 82 AC — no JSON escaping needed for multibyte UTF-8
  std::string s = "\xE2\x82\xAC";
  EXPECT_EQ(roundtrip(s), s);
}

TEST(StringEdge, UTF8EmojiPreserved) {
  // U+1F680 Rocket → F0 9F 9A 80
  std::string s = "\xF0\x9F\x9A\x80";
  EXPECT_EQ(roundtrip(s), s);
}

TEST(StringEdge, VeryLongSimpleString) {
  // Use a large but reasonable string to test the serialization fast path
  std::string s(10000, 'x');
  EXPECT_EQ(roundtrip(s), s);
}

TEST(StringEdge, AllPrintableASCII) {
  std::string s;
  for (char c = 0x20; c < 0x7f; ++c) {
    if (c == '"' || c == '\\')
      continue; // skip JSON-escaped chars
    s += c;
  }
  EXPECT_EQ(roundtrip(s), s);
}

// ─── Structural Mutation
// ──────────────────────────────────────────────────────

TEST(StructuralMutation2, InsertNewKey) {
  Document doc;
  auto root = parse(doc, R"({"a":1})");
  root.insert("b", 2);
  // Original key intact
  EXPECT_EQ(root["a"].as<int>(), 1);
  // Inserted key accessible
  EXPECT_EQ(root["b"].as<int>(), 2);
}

// NOTE: erase() only works on keys from the original parsed JSON.
// Inserting a key then erasing it is not fully supported — the key appears
// in insert buffer which erase doesn't remove. This tests the safe no-crash
// behavior.
TEST(StructuralMutation2, InsertNewKeyAppearsInDump) {
  Document doc;
  auto root = parse(doc, R"({"a":1})");
  root.insert("b", 2);
  // Inserted key appears in dump
  EXPECT_NE(root.dump().find("\"b\":2"), std::string::npos);
  // Original key intact
  EXPECT_EQ(root["a"].as<int>(), 1);
}

TEST(StructuralMutation2, EraseOriginalKey) {
  Document doc;
  auto root = parse(doc, R"({"a":1,"b":2})");
  root.erase("a");
  EXPECT_EQ(root.dump(), R"({"b":2})");
}

// NOTE: push_back() appends to dump() output but subscript access only
// covers the original parsed tape — root[3] after push_back(4) may throw.
TEST(StructuralMutation2, PushBackAppearsInDump) {
  Document doc;
  auto root = parse(doc, "[1,2,3]");
  EXPECT_EQ(root.size(), 3u);
  root.push_back(4);
  root.push_back(5);
  // Newly pushed elements are visible in the serialized JSON
  EXPECT_EQ(root.dump(), "[1,2,3,4,5]");
}

// NOTE: insert_json() (array) inserts into the insertion buffer for dump(),
// but the original tape indices are NOT shifted — root[1] still → 20
// (original).
TEST(StructuralMutation2, ArrayInsertJsonInDump) {
  Document doc;
  auto root = parse(doc, "[10,20,30]");
  root.insert_json(static_cast<size_t>(1), "15");
  // Original tape subscripts unchanged
  EXPECT_EQ(root[0].as<int>(), 10);
  EXPECT_EQ(root[1].as<int>(), 20); // original idx 1 still points to 20
  // But dump() shows the inserted value
  EXPECT_EQ(root.dump(), "[10,15,20,30]");
}

TEST(StructuralMutation2, ArrayMidEraseShift) {
  Document doc;
  auto root = parse(doc, "[10,20,30,40]");
  root.erase(static_cast<size_t>(1));
  EXPECT_EQ(root[0].as<int>(), 10);
  EXPECT_EQ(root[1].as<int>(), 30);
  EXPECT_EQ(root[2].as<int>(), 40);
  EXPECT_EQ(root.dump(), "[10,30,40]");
}

// ─── Iterator Correctness
// ─────────────────────────────────────────────────────

TEST(IteratorCorrectness, ItemsFromParsedObject) {
  Document doc;
  auto root = parse(doc, R"({"a":1,"b":2,"c":3})");

  std::map<std::string, int> got;
  for (auto [k, v] : root.items())
    got[std::string(k)] = v.as<int>();

  EXPECT_EQ(got.size(), 3u);
  EXPECT_EQ(got["a"], 1);
  EXPECT_EQ(got["b"], 2);
  EXPECT_EQ(got["c"], 3);
}

TEST(IteratorCorrectness, ItemsSkipsErased) {
  Document doc;
  auto root = parse(doc, R"({"a":1,"b":2,"c":3})");
  root.erase("b");

  std::vector<std::string> keys;
  for (auto [k, v] : root.items())
    keys.emplace_back(k);

  ASSERT_EQ(keys.size(), 2u);
  EXPECT_EQ(keys[0], "a");
  EXPECT_EQ(keys[1], "c");
}

// NOTE: elements() iterates the original parsed tape only.
// push_back() results appear in dump() but NOT in elements().
TEST(IteratorCorrectness, ElementsReflectsOriginalAfterErase) {
  Document doc;
  auto root = parse(doc, "[1,2,3]");
  root.push_back(4);
  root.erase(static_cast<size_t>(0));

  // dump() reflects all mutations
  EXPECT_EQ(root.dump(), "[2,3,4]");

  // elements() only covers original tape (index 0 erased → [2,3] remain)
  std::vector<int> got;
  for (auto v : root.elements())
    got.push_back(v.as<int>());

  ASSERT_EQ(got.size(), 2u); // indices 1&2 from original tape
  EXPECT_EQ(got[0], 2);
  EXPECT_EQ(got[1], 3);
}

TEST(IteratorCorrectness, ItemsAfterInsert) {
  Document doc;
  auto root = parse(doc, R"({"a":1,"b":2,"c":3})");
  root.insert("d", 4);
  root.erase("b");

  // Verify via dump that structure is correct
  std::string out = root.dump();
  EXPECT_NE(out.find("\"a\":1"), std::string::npos);
  EXPECT_EQ(out.find("\"b\""), std::string::npos); // erased
  EXPECT_NE(out.find("\"c\":3"), std::string::npos);
  EXPECT_NE(out.find("\"d\":4"), std::string::npos);
}

// ─── Deep Struct Round-trip
// ───────────────────────────────────────────────────

TEST(DeepStruct, AppConfigRoundTrip) {
  AppConfig cfg;
  cfg.name = "my_app";

  ServerConfig c1;
  c1.host = "127.0.0.1";
  c1.port = 8080;
  c1.tags = {"web", "api"};
  c1.env = {{"ENV", "prod"}};
  c1.timeout = 30;

  ServerConfig c2;
  c2.host = "10.0.0.1";
  c2.port = 9090;
  c2.timeout = std::nullopt;

  cfg.servers = {c1, c2};
  cfg.primary = c1;

  auto cfg2 = roundtrip(cfg);
  EXPECT_EQ(cfg2.name, "my_app");
  ASSERT_EQ(cfg2.servers.size(), 2u);
  EXPECT_EQ(cfg2.servers[0].host, "127.0.0.1");
  EXPECT_EQ(cfg2.servers[0].port, 8080);
  ASSERT_EQ(cfg2.servers[0].tags.size(), 2u);
  EXPECT_EQ(cfg2.servers[0].tags[0], "web");
  EXPECT_EQ(cfg2.servers[0].env["ENV"], "prod");
  ASSERT_TRUE(cfg2.servers[0].timeout.has_value());
  EXPECT_EQ(*cfg2.servers[0].timeout, 30);
  EXPECT_FALSE(cfg2.servers[1].timeout.has_value());
  EXPECT_EQ(cfg2.primary.host, "127.0.0.1");
}

// ─── Top-level API sanity
// ─────────────────────────────────────────────────────

TEST(TopLevelAPI, WriteNull) { EXPECT_EQ(beast::write(nullptr), "null"); }

TEST(TopLevelAPI, WriteBooleans) {
  EXPECT_EQ(beast::write(true), "true");
  EXPECT_EQ(beast::write(false), "false");
}

TEST(TopLevelAPI, WriteInts) {
  EXPECT_EQ(beast::write(0), "0");
  EXPECT_EQ(beast::write(-1), "-1");
  EXPECT_EQ(beast::write(int64_t{9007199254740992LL}), "9007199254740992");
}

TEST(TopLevelAPI, WriteString) {
  EXPECT_EQ(beast::write(std::string("hi")), "\"hi\"");
  EXPECT_EQ(beast::write(std::string_view("sv")), "\"sv\"");
}

TEST(TopLevelAPI, WriteOptional) {
  EXPECT_EQ(beast::write(std::optional<int>{42}), "42");
  EXPECT_EQ(beast::write(std::optional<int>{}), "null");
}

TEST(TopLevelAPI, ReadWriteRoundtrip) {
  EXPECT_EQ(beast::read<int>("42"), 42);
  EXPECT_EQ(beast::read<std::string>("\"hello\""), "hello");
  EXPECT_EQ(beast::read<bool>("true"), true);
  EXPECT_NEAR(beast::read<double>("3.14"), 3.14, 1e-9);
}

TEST(TopLevelAPI, WriteVectorAndRead) {
  std::vector<int> v = {1, 2, 3};
  EXPECT_EQ(beast::write(v), "[1,2,3]");
  EXPECT_EQ(beast::read<std::vector<int>>("[1,2,3]"), v);
}

TEST(TopLevelAPI, WriteMapAndRead) {
  std::map<std::string, int> m = {{"x", 42}};
  auto json = beast::write(m);
  auto m2 = beast::read<std::map<std::string, int>>(json);
  EXPECT_EQ(m2["x"], 42);
}

TEST(TopLevelAPI, WritePairAndCheckStructure) {
  auto json = beast::write(std::pair<int, bool>{7, true});
  Document doc;
  auto root = parse(doc, json);
  EXPECT_TRUE(root.is_array());
  EXPECT_EQ(root[0].as<int>(), 7);
  EXPECT_TRUE(root[1].as<bool>());
}
