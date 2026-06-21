// fuzz_reflect — macro-free aggregate reflection, hostile-input safety.
//
// The reflected JSON-read and CBOR-decode paths consume untrusted bytes into a
// plain aggregate (NO QBUEM_JSON_FIELDS). They must never read out of bounds,
// hang, or trip ASan/UBSan on malformed input. Two angles:
//   1. decode arbitrary bytes        → read<T> / cbor::decode<T>
//   2. encode → decode round-trip    → whatever decoded must re-encode + re-decode
#include <qbuem_json/qbuem_json.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Deliberately NOT registered with QBUEM_JSON_FIELDS — exercises the reflection
// fallback. Nested aggregate + optional + containers cover the recursive paths.
namespace {
struct RAddr {
  std::string city;
  int zip;
};
struct RUser {
  int64_t id;
  std::string name;
  bool active;
  std::optional<std::string> email;
  std::vector<int> scores;
  std::map<std::string, int> meta;
  RAddr addr;
};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const std::string_view input(reinterpret_cast<const char *>(data), size);

  // 1. Reflected JSON read of hostile text — must throw or partially fill, safely.
  try {
    RUser u = qbuem::read<RUser>(input);
    // Anything that read must serialize and re-read cleanly.
    std::string j = qbuem::write(u);
    try { (void)qbuem::read<RUser>(j); } catch (...) {}
  } catch (const std::exception &) {
  }

  // 1b. Strict read — required-field enforcement + nested recursion on hostile input.
  try {
    (void)qbuem::read_strict<RUser>(input);
  } catch (const std::exception &) {
  }

  // 2. Reflected CBOR decode of hostile bytes — bounds-checked reader.
  try {
    RUser u = qbuem::cbor::decode<RUser>(input);
    std::string enc = qbuem::cbor::encode(u);
    try { (void)qbuem::cbor::decode<RUser>(enc); } catch (...) {}
  } catch (const std::exception &) {
  }

  // 3. (ptr,len) CBOR overload — same path, different entry.
  try {
    (void)qbuem::cbor::decode<RUser>(data, size);
  } catch (...) {
  }

  return 0;
}
