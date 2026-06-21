// WebAssembly (Emscripten/Embind) surface for qbuem-json.
//
// Exposes the operations a browser most wants from a native JSON engine and that
// the platform does NOT provide itself: fast RFC 8259 validation, compact
// re-serialization (minify), RFC 8785-style canonicalization (for hashing /
// signing), and JSONPath (RFC 9535) querying. All operate on arbitrary JSON
// strings — no schema needed.
//
// Build: see CMakeLists.txt / build.sh in this directory. Produces an ES module
// (qbuem_json.mjs + qbuem_json.wasm) usable from the browser and Node.
#include <qbuem_json/qbuem_json.hpp>

#include <emscripten/bind.h>

#include <string>

namespace {

// Returns true iff `json` is well-formed per RFC 8259 (strict UTF-8). Never throws.
bool validate(const std::string &json) {
  try {
    qbuem::rfc8259::validate(json);
    return true;
  } catch (...) {
    return false;
  }
}

// Compact re-serialization (strips insignificant whitespace). Throws on invalid
// input — Embind surfaces the throw as a JS exception.
std::string minify(const std::string &json) {
  qbuem::Document doc;
  qbuem::Value root = qbuem::parse(doc, json);
  return root.dump();
}

// Pretty-print with `indent` spaces per level.
std::string prettify(const std::string &json, int indent) {
  qbuem::Document doc;
  qbuem::Value root = qbuem::parse(doc, json);
  return root.dump(indent);
}

// Deterministic canonical form (sorted keys, shortest numbers) — for hashing,
// signing, content-addressing. RFC 8785 (JCS) structure. Strict-parses input.
std::string canonicalize(const std::string &json) {
  return qbuem::canonicalize(json);
}

// JSONPath (RFC 9535 structural selectors). Returns a JSON array string of the
// selected values, in document order. Throws on a malformed query.
std::string query(const std::string &json, const std::string &path) {
  qbuem::Document doc;
  qbuem::Value root = qbuem::parse(doc, json);
  auto matches = qbuem::query(root, path);
  std::string out = "[";
  for (std::size_t i = 0; i < matches.size(); ++i) {
    if (i) out += ',';
    out += matches[i].dump();
  }
  out += ']';
  return out;
}

} // namespace

EMSCRIPTEN_BINDINGS(qbuem_json) {
  emscripten::function("validate", &validate);
  emscripten::function("minify", &minify);
  emscripten::function("prettify", &prettify);
  emscripten::function("canonicalize", &canonicalize);
  emscripten::function("query", &query);
}
