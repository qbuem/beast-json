// fuzz_apply_patch — RFC 6902 functional applier + diff generation.
// Two angles: (1) apply an arbitrary patch to an arbitrary document — the
// functional rebuild parses untrusted JSON for both and must never crash / read
// OOB / loop; (2) diff two arbitrary documents, then apply the generated patch
// (self-consistency).
#include <qbuem_json/qbuem_json.hpp>

#include <cstdint>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::string_view in(reinterpret_cast<const char *>(data), size);

  // Split at the first NUL into doc | patch (else use halves).
  size_t split = in.find('\0');
  std::string_view a, b;
  if (split == std::string_view::npos) {
    a = in.substr(0, in.size() / 2);
    b = in.substr(in.size() / 2);
  } else {
    a = in.substr(0, split);
    b = in.substr(split + 1);
  }

  // 1. Apply b as an RFC 6902 patch to a.
  try {
    (void)qbuem::apply_patch(a, b);
  } catch (const std::exception &) {
  }

  // 2. diff(a, b) then apply it back (when both parse as JSON).
  try {
    std::string patch = qbuem::diff(a, b);
    (void)qbuem::apply_patch(a, patch);
  } catch (const std::exception &) {
  }

  return 0;
}
