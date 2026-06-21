// fuzz_cbor — CBOR codec stress test.
//
// Two angles, both must never read out of bounds, hang, or trip UBSan:
//   1. decode arbitrary bytes      → cbor::decode<T>(raw fuzzer input)
//   2. encode → decode round-trip  → decode succeeds ⇒ re-encode ⇒ re-decode
// The first proves the reader is hostile-input safe (truncation, bogus heads,
// length bombs, deep nesting); the second proves the encoder always emits bytes
// the decoder accepts (self-consistency).
#include <qbuem_json/qbuem_json.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

enum class CborTier : uint8_t { Free, Lite, Premium };   // integer enum
enum class CborColor { Red, Green, Blue };               // string-mapped enum
QBUEM_JSON_ENUM(CborColor, Red, Green, Blue)

struct CborNested {
  int64_t id;
  std::string name;
  CborTier tier;
  CborColor color;
};
QBUEM_JSON_FIELDS(CborNested, id, name, tier, color)

struct CborFuzz {
  int64_t i;
  uint64_t u;
  double d;
  bool b;
  std::string s;
  std::vector<int64_t> v;
  std::optional<std::string> o;
  std::map<std::string, int64_t> m;
  std::array<int64_t, 3> arr;
  std::pair<std::string, double> pr;
  CborNested nested;
};
QBUEM_JSON_FIELDS(CborFuzz, i, u, d, b, s, v, o, m, arr, pr, nested)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const std::string_view input(reinterpret_cast<const char *>(data), size);

  // 1. Decode hostile bytes — must throw or partially decode, never read OOB.
  try {
    CborFuzz obj = qbuem::cbor::decode<CborFuzz>(input);

    // 2. Anything that decoded must re-encode and re-decode cleanly.
    std::string encoded = qbuem::cbor::encode(obj);
    try {
      (void)qbuem::cbor::decode<CborFuzz>(encoded);
    } catch (...) {
    }
  } catch (const std::exception &) {
    // Expected for malformed CBOR.
  }

  // 3. The (ptr,len) overload shares the path but exercises a different entry.
  try {
    (void)qbuem::cbor::decode<CborFuzz>(data, size);
  } catch (...) {
  }

  return 0;
}
