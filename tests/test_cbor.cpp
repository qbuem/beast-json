// CBOR encode tests: serialize QBUEM_JSON_FIELDS structs to CBOR and verify the
// bytes with an independent minimal reference decoder (the encoder must produce
// valid, correctly-typed RFC 8949 data items).
#include <gtest/gtest.h>

#include "qbuem_json/qbuem_json.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// ── Minimal reference CBOR reader (independent of the encoder) ────────────────
namespace {
struct Rd {
  const uint8_t *p, *e;
  uint8_t major() const { return *p >> 5; }
  uint64_t head() {
    uint8_t info = *p++ & 31; uint64_t v = info;
    if (info == 24) v = *p++;
    else if (info == 25) { v = (uint64_t(p[0]) << 8) | p[1]; p += 2; }
    else if (info == 26) { v = 0; for (int i = 0; i < 4; ++i) v = (v << 8) | *p++; }
    else if (info == 27) { v = 0; for (int i = 0; i < 8; ++i) v = (v << 8) | *p++; }
    return v;
  }
  int64_t rd_int() { uint8_t m = major(); uint64_t a = head(); return m == 1 ? -1 - int64_t(a) : int64_t(a); }
  std::string rd_text() { uint64_t n = head(); std::string s((const char *)p, n); p += n; return s; }
  double rd_double() { ++p; uint64_t b = 0; for (int i = 0; i < 8; ++i) b = (b << 8) | *p++; double d; std::memcpy(&d, &b, 8); return d; }
  bool rd_bool() { return (*p++ & 31) == 21; }
  bool is_null() { if (*p == 0xf6) { ++p; return true; } return false; }
  bool at_break() const { return *p == 0xff; }
  void skip_break() { ++p; }
  bool open_indef_map() { if (*p == 0xbf) { ++p; return true; } return false; }
  uint64_t open_map() { return head(); }   // definite-length map → entry count
  uint64_t open_array() { return head(); }
};

// ── Minimal reference CBOR *writer* (independent of the encoder) ──────────────
// Used to forge payloads a different encoder (e.g. JS cbor-x) would emit —
// definite-length maps/arrays, smallest-int forms, half/single floats — and feed
// them to qbuem::cbor::decode to prove cross-encoder interop.
struct Wr {
  std::string b;
  void head(uint8_t major, uint64_t v) {
    uint8_t m = uint8_t(major << 5);
    if (v < 24) b.push_back(char(m | v));
    else if (v < 256) { b.push_back(char(m | 24)); b.push_back(char(v)); }
    else if (v < 65536) { b.push_back(char(m | 25)); b.push_back(char(v >> 8)); b.push_back(char(v)); }
    else { b.push_back(char(m | 26)); for (int s = 24; s >= 0; s -= 8) b.push_back(char(v >> s)); }
  }
  void u(uint64_t v) { head(0, v); }
  void i(int64_t v) { if (v < 0) head(1, uint64_t(-1 - v)); else head(0, uint64_t(v)); }
  void str(const std::string &s) { head(3, s.size()); b += s; }
  void def_map(uint64_t n) { head(5, n); }        // definite-length map
  void def_arr(uint64_t n) { head(4, n); }        // definite-length array
  void key(const char *k) { str(k); }
  void f64(double d) { uint64_t bits; std::memcpy(&bits, &d, 8); b.push_back(char(0xfb)); for (int s = 56; s >= 0; s -= 8) b.push_back(char(bits >> s)); }
  void f32(float f) { uint32_t bits; std::memcpy(&bits, &f, 4); b.push_back(char(0xfa)); for (int s = 24; s >= 0; s -= 8) b.push_back(char(bits >> s)); }
  void f16(uint16_t h) { b.push_back(char(0xf9)); b.push_back(char(h >> 8)); b.push_back(char(h)); }
  void boolean(bool v) { b.push_back(char(v ? 0xf5 : 0xf4)); }
  void null() { b.push_back(char(0xf6)); }
};

} // namespace

// NB: QBUEM_JSON_FIELDS must be invoked in the SAME scope as the struct so the
// generated (de)serializers are found by ADL — true for both JSON and CBOR.
struct Inner { int64_t a; std::string b; };
QBUEM_JSON_FIELDS(Inner, a, b)

struct Msg {
  int64_t id; std::string name; double score; bool active;
  std::optional<int64_t> opt; std::vector<int64_t> nums; Inner inner;
};
QBUEM_JSON_FIELDS(Msg, id, name, score, active, opt, nums, inner)

struct Bag {
  std::map<std::string, int64_t> dict;
  std::array<int64_t, 3> trip;
  std::pair<std::string, int64_t> kv;
};
QBUEM_JSON_FIELDS(Bag, dict, trip, kv)

struct U64 { uint64_t big; };
QBUEM_JSON_FIELDS(U64, big)

TEST(Cbor, EncodeStructFullRoundTrip) {
  Msg m{ 42, "hero", 3.5, true, std::optional<int64_t>{7}, {10, 20, 30}, Inner{ -1, "z" } };
  std::string bytes = qbuem::cbor::encode(m);
  ASSERT_FALSE(bytes.empty());
  ASSERT_EQ((uint8_t)bytes[0], 0xa7); // definite-length map of 7 fields (v1.1.1)

  Rd r{ (const uint8_t *)bytes.data(), (const uint8_t *)bytes.data() + bytes.size() };
  EXPECT_EQ(r.open_map(), 7u);
  EXPECT_EQ(r.rd_text(), "id");     EXPECT_EQ(r.rd_int(), 42);
  EXPECT_EQ(r.rd_text(), "name");   EXPECT_EQ(r.rd_text(), "hero");
  EXPECT_EQ(r.rd_text(), "score");  EXPECT_DOUBLE_EQ(r.rd_double(), 3.5);
  EXPECT_EQ(r.rd_text(), "active"); EXPECT_TRUE(r.rd_bool());
  EXPECT_EQ(r.rd_text(), "opt");    EXPECT_EQ(r.rd_int(), 7);
  EXPECT_EQ(r.rd_text(), "nums");
  EXPECT_EQ(r.open_array(), 3u);
  EXPECT_EQ(r.rd_int(), 10); EXPECT_EQ(r.rd_int(), 20); EXPECT_EQ(r.rd_int(), 30);
  EXPECT_EQ(r.rd_text(), "inner");
  EXPECT_EQ(r.open_map(), 2u);
  EXPECT_EQ(r.rd_text(), "a"); EXPECT_EQ(r.rd_int(), -1); // negative int
  EXPECT_EQ(r.rd_text(), "b"); EXPECT_EQ(r.rd_text(), "z");
  EXPECT_EQ(r.p, r.e);                                    // consumed exactly (no break bytes)
}

TEST(Cbor, EncodeOptionalEmptyIsNull) {
  Msg m{ 1, "x", 1.0, false, std::nullopt, {}, Inner{ 0, "" } };
  std::string b = qbuem::cbor::encode(m);
  Rd r{ (const uint8_t *)b.data(), (const uint8_t *)b.data() + b.size() };
  r.open_map();
  r.rd_text(); r.rd_int();    // id
  r.rd_text(); r.rd_text();   // name
  r.rd_text(); r.rd_double(); // score
  r.rd_text(); r.rd_bool();   // active
  EXPECT_EQ(r.rd_text(), "opt");
  EXPECT_TRUE(r.is_null());   // nullopt → CBOR null
  EXPECT_EQ(r.rd_text(), "nums");
  EXPECT_EQ(r.open_array(), 0u); // empty vector → empty array
}

// ── decode: full encode → decode round-trip recovers every field ─────────────
TEST(Cbor, RoundTripRecoversAllFields) {
  Msg in{ 42, "hero", 3.5, true, std::optional<int64_t>{7}, {10, 20, 30}, Inner{ -1, "z" } };
  std::string bytes = qbuem::cbor::encode(in);
  Msg out = qbuem::cbor::decode<Msg>(bytes);
  EXPECT_EQ(out.id, 42);
  EXPECT_EQ(out.name, "hero");
  EXPECT_DOUBLE_EQ(out.score, 3.5);
  EXPECT_TRUE(out.active);
  ASSERT_TRUE(out.opt.has_value());
  EXPECT_EQ(*out.opt, 7);
  EXPECT_EQ(out.nums, (std::vector<int64_t>{10, 20, 30}));
  EXPECT_EQ(out.inner.a, -1);
  EXPECT_EQ(out.inner.b, "z");
}

TEST(Cbor, RoundTripNulloptAndEmpty) {
  Msg in{ 1, "x", 1.0, false, std::nullopt, {}, Inner{ 0, "" } };
  Msg out = qbuem::cbor::decode<Msg>(qbuem::cbor::encode(in));
  EXPECT_EQ(out.id, 1);
  EXPECT_FALSE(out.active);
  EXPECT_FALSE(out.opt.has_value()); // CBOR null → nullopt
  EXPECT_TRUE(out.nums.empty());
}

// ── decode: interop with a foreign encoder (definite-length map, smallest ints,
// out-of-order keys, an unknown extra key, single/half floats) ──────────────
TEST(Cbor, DecodeForeignDefiniteMap) {
  Wr w;
  w.def_map(8);                          // definite-length map, 8 entries
  w.key("name");  w.str("zed");          // keys out of struct order
  w.key("id");    w.i(-5);
  w.key("extra"); w.i(999);              // unknown key → must be skipped
  w.key("active");w.boolean(true);
  w.key("score"); w.i(4);                // integer where a double field is expected
  w.key("opt");   w.null();              // null → nullopt
  w.key("nums");  w.def_arr(2); w.u(7); w.u(8);
  w.key("inner"); w.def_map(2); w.key("b"); w.str("q"); w.key("a"); w.i(3);

  Msg m = qbuem::cbor::decode<Msg>(w.b);
  EXPECT_EQ(m.id, -5);
  EXPECT_EQ(m.name, "zed");
  EXPECT_DOUBLE_EQ(m.score, 4.0);        // int decoded into double
  EXPECT_TRUE(m.active);
  EXPECT_FALSE(m.opt.has_value());
  EXPECT_EQ(m.nums, (std::vector<int64_t>{7, 8}));
  EXPECT_EQ(m.inner.a, 3);
  EXPECT_EQ(m.inner.b, "q");
}

// ── Positional fast path (v1.1.4): in-order, fully-reversed, and fast-prefix-
// then-reordered maps must ALL decode identically. The fast path consumes the
// in-order prefix; the first out-of-order key flips it to the hash-dispatch
// fallback. Build a foreign map per ordering and assert the same result. ───────
static void put_field(Wr &w, const char *k, const Msg &m) {
  std::string_view key(k);
  w.key(k);
  if (key == "id")          w.i(m.id);
  else if (key == "name")   w.str(m.name);
  else if (key == "score")  w.f64(m.score);
  else if (key == "active") w.boolean(m.active);
  else if (key == "opt")    { if (m.opt) w.i(*m.opt); else w.null(); }
  else if (key == "nums")   { w.def_arr(m.nums.size()); for (auto n : m.nums) w.i(n); }
  else /* inner */          { w.def_map(2); w.key("a"); w.i(m.inner.a); w.key("b"); w.str(m.inner.b); }
}
static Msg decode_in_order(const Msg &src, std::vector<const char *> order) {
  Wr w; w.def_map(order.size());
  for (auto *k : order) put_field(w, k, src);
  return qbuem::cbor::decode<Msg>(w.b);
}
TEST(Cbor, PositionalFastPathOrderings) {
  const Msg src{ 7, "kai", 2.25, true, std::optional<int64_t>{99}, {3, 4, 5}, Inner{ -8, "qq" } };
  auto check = [&](const Msg &m) {
    EXPECT_EQ(m.id, 7);
    EXPECT_EQ(m.name, "kai");
    EXPECT_DOUBLE_EQ(m.score, 2.25);
    EXPECT_TRUE(m.active);
    ASSERT_TRUE(m.opt.has_value());
    EXPECT_EQ(*m.opt, 99);
    EXPECT_EQ(m.nums, (std::vector<int64_t>{3, 4, 5}));
    EXPECT_EQ(m.inner.a, -8);
    EXPECT_EQ(m.inner.b, "qq");
  };
  // 1. Exact struct order — the all-fast path consumes every field.
  check(decode_in_order(src, {"id", "name", "score", "active", "opt", "nums", "inner"}));
  // 2. Fully reversed — fast path bails on the very first key (full fallback).
  check(decode_in_order(src, {"inner", "nums", "opt", "active", "score", "name", "id"}));
  // 3. Fast prefix then reorder — fast consumes id,name,score, then the next key
  //    (opt, not active) flips to the hash fallback for the remaining four.
  check(decode_in_order(src, {"id", "name", "score", "opt", "active", "nums", "inner"}));
  // 4. Single swap in the middle.
  check(decode_in_order(src, {"id", "name", "active", "score", "opt", "nums", "inner"}));
}

TEST(Cbor, DecodeHalfAndSingleFloat) {
  // score via half-float 0x4200 = 3.0; then via single-precision.
  { Wr w; w.def_map(1); w.key("score"); w.f16(0x4200);
    Msg m = qbuem::cbor::decode<Msg>(w.b); EXPECT_DOUBLE_EQ(m.score, 3.0); }
  { Wr w; w.def_map(1); w.key("score"); w.f32(2.5f);
    Msg m = qbuem::cbor::decode<Msg>(w.b); EXPECT_DOUBLE_EQ(m.score, 2.5); }
}

// ── STL container coverage: map, fixed array, pair ───────────────────────────
TEST(Cbor, RoundTripStlContainers) {
  Bag in{ { {"a", 1}, {"b", 2} }, { 7, 8, 9 }, { "k", 42 } };
  Bag out = qbuem::cbor::decode<Bag>(qbuem::cbor::encode(in));
  EXPECT_EQ(out.dict.size(), 2u);
  EXPECT_EQ(out.dict.at("a"), 1);
  EXPECT_EQ(out.dict.at("b"), 2);
  EXPECT_EQ(out.trip, (std::array<int64_t, 3>{7, 8, 9}));
  EXPECT_EQ(out.kv.first, "k");
  EXPECT_EQ(out.kv.second, 42);
}

// ── Unsigned 64-bit round-trip (raw arg path, no int64 narrowing) ────────────
TEST(Cbor, RoundTripUnsigned64) {
  U64 in{ 0xFFFFFFFFFFFFFFFAull };       // top bit set — would corrupt via int64
  U64 out = qbuem::cbor::decode<U64>(qbuem::cbor::encode(in));
  EXPECT_EQ(out.big, 0xFFFFFFFFFFFFFFFAull);
}

// ── Malformed input is rejected, never read out of bounds ────────────────────
TEST(Cbor, TruncatedInputThrows) {
  std::string bytes = qbuem::cbor::encode(Msg{ 1, "abc", 1.0, true, std::nullopt, {1, 2}, Inner{ 0, "" } });
  for (size_t cut = 1; cut < bytes.size(); ++cut) {
    std::string truncated = bytes.substr(0, cut);
    // Either throws parse_error or decodes a partial object, but must not crash
    // or read past the buffer (ASan/UBSan in CI proves the latter).
    try { (void)qbuem::cbor::decode<Msg>(truncated); }
    catch (const qbuem::parse_error &) { /* expected for most cut points */ }
  }
}

TEST(Cbor, TypeMismatchThrows) {
  Wr w; w.def_map(1); w.key("name"); w.u(123); // number where string expected
  EXPECT_THROW((void)qbuem::cbor::decode<Msg>(w.b), qbuem::parse_error);
}
