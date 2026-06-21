// fuzz_sax — SAX event visitor over arbitrary (fuzzer-controlled) JSON.
// visit() recurses to the JSON's nesting depth (bounded by the parser's depth
// limit) and reuses the already-fuzzed Value iteration; this target confirms the
// recursive walk + handler dispatch stay safe on hostile input, including an
// early-abort handler that stops mid-walk.
#include <qbuem_json/qbuem_json.hpp>

#include <cstdint>
#include <string>

namespace {
// Touches every event and aborts pseudo-randomly to exercise early-exit paths.
struct FuzzHandler : qbuem::sax_handler {
  uint32_t budget;
  explicit FuzzHandler(uint32_t b) : budget(b) {}
  bool tick() { return budget-- != 0; }
  bool start_object() { return tick(); }
  bool end_object(size_t) { return tick(); }
  bool start_array() { return tick(); }
  bool end_array(size_t) { return tick(); }
  bool key(std::string_view k) { sink ^= k.size(); return tick(); }
  bool string(std::string_view s) { sink ^= s.size(); return tick(); }
  bool integer(int64_t i) { sink ^= static_cast<size_t>(i); return tick(); }
  bool real(double) { return tick(); }
  bool boolean(bool) { return tick(); }
  bool null() { return tick(); }
  size_t sink = 0;
};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::string_view in(reinterpret_cast<const char *>(data), size);

  // Derive an abort budget from the first byte so some walks stop early.
  const uint32_t budget = size ? (static_cast<uint32_t>(data[0]) * 4u + 1u) : 1u;

  try {
    qbuem::Document doc;
    qbuem::Value root = qbuem::parse(doc, in);
    FuzzHandler h(budget);
    (void)qbuem::visit(root, h);
  } catch (const std::exception &) {
  }
  return 0;
}
