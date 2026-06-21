// Field rename (roadmap Tier 1): a field may be written `(member, "jsonKey")` to
// map it to a different wire key. Skipping is by omission — a member not listed
// in QBUEM_JSON_FIELDS is neither serialized nor read. Verified across every
// engine (DOM read/write, fuse, CBOR).
#include <gtest/gtest.h>

#include "qbuem_json/qbuem_json.hpp"

#include <cstdint>
#include <string>

struct User {
  int64_t     id;
  std::string user_name;   // → "userName"
  int         level;       // bare key
  int         internal;    // NOT registered → skipped
};
QBUEM_JSON_FIELDS(User, (id, "userId"), (user_name, "userName"), level)

// A renamed field nested inside another struct, plus a renamed long key (>8 B,
// exercises the full-compare path in the CBOR/JSON verify).
struct Outer {
  User                 owner;
  std::string          description_text;  // → "description"
};
QBUEM_JSON_FIELDS(Outer, (owner, "user"), (description_text, "description"))

TEST(FieldRename, JsonUsesRenamedKeys) {
  const std::string j = qbuem::write(User{7, "neo", 42, 999});
  EXPECT_NE(j.find("\"userId\":7"), std::string::npos);
  EXPECT_NE(j.find("\"userName\":\"neo\""), std::string::npos);
  EXPECT_NE(j.find("\"level\":42"), std::string::npos);
  EXPECT_EQ(j.find("internal"), std::string::npos); // skipped by omission
  EXPECT_EQ(j.find("user_name"), std::string::npos); // C++ name not leaked
}

TEST(FieldRename, DomReadRenamedKeys) {
  auto a = qbuem::read<User>(
      R"({"userId":11,"userName":"trinity","level":5,"internal":123})");
  EXPECT_EQ(a.id, 11);
  EXPECT_EQ(a.user_name, "trinity");
  EXPECT_EQ(a.level, 5);
  EXPECT_EQ(a.internal, 0); // skipped → left at default, not 123
}

TEST(FieldRename, FuseReadRenamedKeys) {
  auto f = qbuem::fuse<User>(
      R"({"userId":3,"userName":"morpheus","level":9})");
  EXPECT_EQ(f.id, 3);
  EXPECT_EQ(f.user_name, "morpheus");
  EXPECT_EQ(f.level, 9);
}

TEST(FieldRename, CborRoundTripRenamedKeys) {
  User u{7, "neo", 42, 999};
  auto c = qbuem::cbor::decode<User>(qbuem::cbor::encode(u));
  EXPECT_EQ(c.id, 7);
  EXPECT_EQ(c.user_name, "neo");
  EXPECT_EQ(c.level, 42);
  // The CBOR map must carry the renamed text key, not the C++ member name.
  std::string bytes = qbuem::cbor::encode(u);
  EXPECT_NE(bytes.find("userName"), std::string::npos);
  EXPECT_EQ(bytes.find("user_name"), std::string::npos);
}

TEST(FieldRename, NestedAndLongRenamedKeys) {
  Outer o{ User{1, "a", 2, 0}, "hello world" };
  const std::string j = qbuem::write(o);
  EXPECT_NE(j.find("\"user\":"), std::string::npos);
  EXPECT_NE(j.find("\"description\":\"hello world\""), std::string::npos);

  auto dom = qbuem::read<Outer>(j);
  EXPECT_EQ(dom.owner.user_name, "a");
  EXPECT_EQ(dom.description_text, "hello world");

  auto cb = qbuem::cbor::decode<Outer>(qbuem::cbor::encode(o));
  EXPECT_EQ(cb.owner.id, 1);
  EXPECT_EQ(cb.description_text, "hello world");
}
