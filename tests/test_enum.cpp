// Enum support (roadmap Tier 1): an enum serializes as its underlying integer by
// default, or as its value name when registered with QBUEM_JSON_ENUM — across
// every engine (DOM read/write, fuse, CBOR).
#include <gtest/gtest.h>

#include "qbuem_json/qbuem_json.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Integer enum (no registration) — encodes as the underlying value.
enum class Dir : int { North = 0, East = 1, South = 2, West = 3 };

// Small underlying type, to confirm the cast path is width-correct.
enum class Tier : std::uint8_t { Free = 0, Lite = 1, Premium = 2 };

// String-mapped enum (opt-in).
enum class Color { Red, Green, Blue };
QBUEM_JSON_ENUM(Color, Red, Green, Blue)

struct Item {
  int64_t id;
  Color color;            // string
  Dir facing;             // integer
  std::optional<Color> accent;
  std::vector<Color> palette;
};
QBUEM_JSON_FIELDS(Item, id, color, facing, accent, palette)

TEST(Enum, IntegerEnumDefaultEncoding) {
  // fuse() is for structs; bare-scalar round-trips use DOM/CBOR. The enum-in-
  // struct fuse path is covered by NestedInStructAllEngines below.
  EXPECT_EQ(qbuem::write(Dir::West), "3");
  EXPECT_EQ(qbuem::read<Dir>("2"), Dir::South);
  EXPECT_EQ(qbuem::cbor::decode<Dir>(qbuem::cbor::encode(Dir::South)), Dir::South);
}

TEST(Enum, SmallUnderlyingType) {
  EXPECT_EQ(qbuem::write(Tier::Premium), "2");
  EXPECT_EQ(qbuem::cbor::decode<Tier>(qbuem::cbor::encode(Tier::Lite)), Tier::Lite);
}

TEST(Enum, StringEnumName) {
  EXPECT_EQ(qbuem::write(Color::Green), "\"Green\"");
  EXPECT_EQ(qbuem::read<Color>("\"Blue\""), Color::Blue);
}

TEST(Enum, StringEnumCborIsText) {
  std::string bytes = qbuem::cbor::encode(Color::Blue);
  ASSERT_FALSE(bytes.empty());
  EXPECT_EQ((uint8_t)bytes[0] >> 5, 3u); // CBOR major type 3 = text string
  EXPECT_EQ(qbuem::cbor::decode<Color>(bytes), Color::Blue);
}

TEST(Enum, NestedInStructAllEngines) {
  Item in{ 7, Color::Green, Dir::West, Color::Red, { Color::Blue, Color::Green } };

  const std::string j = qbuem::write(in);
  EXPECT_NE(j.find("\"color\":\"Green\""), std::string::npos);
  EXPECT_NE(j.find("\"facing\":3"), std::string::npos);
  EXPECT_NE(j.find("\"accent\":\"Red\""), std::string::npos);

  auto dom = qbuem::read<Item>(j);
  EXPECT_EQ(dom.color, Color::Green);
  EXPECT_EQ(dom.facing, Dir::West);
  ASSERT_TRUE(dom.accent.has_value());
  EXPECT_EQ(*dom.accent, Color::Red);
  EXPECT_EQ(dom.palette, (std::vector<Color>{ Color::Blue, Color::Green }));

  auto fus = qbuem::fuse<Item>(j);
  EXPECT_EQ(fus.color, Color::Green);
  EXPECT_EQ(fus.palette.size(), 2u);

  auto cb = qbuem::cbor::decode<Item>(qbuem::cbor::encode(in));
  EXPECT_EQ(cb.color, Color::Green);
  EXPECT_EQ(cb.facing, Dir::West);
  ASSERT_TRUE(cb.accent.has_value());
  EXPECT_EQ(*cb.accent, Color::Red);
  EXPECT_EQ(cb.palette[0], Color::Blue);
}

TEST(Enum, OptionalNulloptStringEnum) {
  Item in{ 1, Color::Red, Dir::North, std::nullopt, {} };
  auto cb = qbuem::cbor::decode<Item>(qbuem::cbor::encode(in));
  EXPECT_FALSE(cb.accent.has_value());
  auto dom = qbuem::read<Item>(qbuem::write(in));
  EXPECT_FALSE(dom.accent.has_value());
}

TEST(Enum, UnknownStringOnCborThrows) {
  // A text string that maps to no enumerator must throw, not silently corrupt.
  std::string bad;
  bad.push_back((char)0x64);           // text(4)
  bad += "Cyan";                        // not a Color
  EXPECT_THROW((void)qbuem::cbor::decode<Color>(bad), qbuem::parse_error);
}

TEST(Enum, UnknownStringOnDomLeavesDefault) {
  // DOM read of an unknown name leaves the field at its prior/default value.
  Color c = Color::Green;
  c = qbuem::read<Color>("\"Cyan\""); // unknown → unchanged from a fresh default
  // read<T> default-constructs first (Color{} == Red), then leaves it on miss.
  EXPECT_EQ(c, Color{});
}
