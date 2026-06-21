// fuzz_jsonpath — JSONPath query parser + evaluator.
// Splits the input into a query string and a JSON document (at the first NUL, or
// uses a fixed document otherwise) so the fuzzer drives BOTH the untrusted query
// parser and traversal over arbitrary JSON. Must never crash / read OOB / loop.
#include <qbuem_json/qbuem_json.hpp>

#include <cstdint>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::string_view in(reinterpret_cast<const char *>(data), size);

  // Use the bytes up to the first NUL as the query; the rest as a JSON document.
  size_t split = in.find('\0');
  std::string_view query = (split == std::string_view::npos) ? in : in.substr(0, split);
  std::string_view jsondoc =
      (split == std::string_view::npos) ? std::string_view("{\"a\":[1,2,{\"b\":3}],\"c\":{\"a\":4}}")
                                        : in.substr(split + 1);

  // 1. Query a fixed, well-formed document — exercises the path parser hard.
  {
    qbuem::Document d;
    static const char *kDoc =
        "{\"store\":{\"book\":[{\"t\":1},{\"t\":2}],\"x\":{\"y\":3}},\"n\":[0,1,2,3,4]}";
    try {
      qbuem::Value root = qbuem::parse(d, kDoc);
      auto r = qbuem::query(root, query);
      (void)r;
    } catch (const std::exception &) {
    }
  }

  // 2. Query an arbitrary (fuzzer-controlled) document.
  {
    qbuem::Document d;
    try {
      qbuem::Value root = qbuem::parse(d, jsondoc);
      auto r = qbuem::query(root, query);
      (void)r;
    } catch (const std::exception &) {
    }
  }

  return 0;
}
