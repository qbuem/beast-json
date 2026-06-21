// fuzz_ndjson — NDJSON / JSON Lines reader.
// The line splitter feeds arbitrary sub-views of the input to the parser, so the
// new surface to prove safe is: blank-line / CRLF handling, and parsing a record
// that is NOT the start of the buffer and whose end is mid-buffer (SIMD over-read
// must stay in bounds). Records that fail to parse throw and are swallowed.
#include <qbuem_json/qbuem_json.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct NdRec {
  int64_t id;
  std::string name;
  std::vector<int64_t> tags;
};
QBUEM_JSON_FIELDS(NdRec, id, name, tags)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const std::string_view input(reinterpret_cast<const char *>(data), size);

  // 1. Typed, callback (bounded) — a malformed record throws mid-stream.
  try {
    qbuem::read_lines<NdRec>(input, [](NdRec &&) {});
  } catch (const std::exception &) {
  }

  // 2. Collect-into-vector entry point.
  try {
    auto v = qbuem::read_lines<NdRec>(input);
    (void)v;
  } catch (const std::exception &) {
  }

  return 0;
}
