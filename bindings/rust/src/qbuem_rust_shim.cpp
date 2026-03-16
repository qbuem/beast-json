#include "qbuem_rust_shim.hpp"
#include "qbuem-json/src/lib.rs.h"
#include <stdexcept>

namespace qbuem::rust {

// Both TapeNode (from CXX) and qbuem::TapeNode should be identical
// Note: In qbuem_json.hpp, TapeNode is in global namespace or qbuem?
// Let's check the previous read_file output.
// L330: struct TapeNode { ... } -- it was inside namespace qbuem { ... } in the file?
// No, the file read started with // TapeNode -- 8 bytes (compaction)
// I will use sizeof(::TapeNode) if qbuem::TapeNode fails.
// But the error said "no member named 'TapeNode' in namespace 'qbuem'".
// This means it's likely in global namespace.

static_assert(sizeof(TapeNode) == 8, "TapeNode size mismatch");

std::unique_ptr<RustDocument> create_doc() {
    return std::make_unique<RustDocument>();
}

void parse(RustDocument &doc, ::rust::Str json) {
    doc.parse(json);
}

const TapeNode* get_tape_ptr(const RustDocument &doc) {
    // We just need the raw pointer to the first element
    return reinterpret_cast<const TapeNode*>(doc.doc.state()->tape.base);
}

size_t get_tape_size(const RustDocument &doc) {
    return doc.doc.state()->tape.size();
}

const uint8_t* get_source_ptr(const RustDocument &doc) {
    return reinterpret_cast<const uint8_t*>(doc.source.data());
}

size_t get_source_size(const RustDocument &doc) {
    return doc.source.size();
}

::rust::String dump(const RustDocument &doc, uint32_t idx, int32_t indent) {
    return ::rust::String(doc.get_value(idx).dump(indent));
}

void dump_to(const RustDocument &doc, uint32_t idx, int32_t indent, ::rust::String &out) {
    thread_local std::string buf;
    auto v = doc.get_value(idx);
    if (indent > 0) {
        buf = v.dump(indent);
    } else {
        v.dump(buf);
    }
    out = ::rust::String(buf);
}

uint32_t at_path_idx(const RustDocument &doc, uint32_t idx, ::rust::Str path) {
    auto v = doc.get_value(idx).at(std::string_view(path.data(), path.size()));
    return v.is_valid() ? v.index() : 0xFFFFFFFFu;
}

void set_null(RustDocument &doc, uint32_t idx) { doc.get_value(idx).set(nullptr); }
void set_bool(RustDocument &doc, uint32_t idx, bool b) { doc.get_value(idx).set(b); }
void set_int(RustDocument &doc, uint32_t idx, int64_t i) { doc.get_value(idx).set(i); }
void set_double(RustDocument &doc, uint32_t idx, double d) { doc.get_value(idx).set(d); }
void set_string(RustDocument &doc, uint32_t idx, ::rust::Str s) { 
    doc.get_value(idx).set(std::string_view(s.data(), s.size())); 
}

void insert_raw(RustDocument &doc, uint32_t idx, ::rust::Str key, ::rust::Str raw_json) {
    doc.get_value(idx).insert(std::string(key), std::string(raw_json));
}
void erase_key(RustDocument &doc, uint32_t idx, ::rust::Str key) { 
    doc.get_value(idx).erase(std::string(key)); 
}
void erase_idx(RustDocument &doc, uint32_t idx, size_t idx_to_erase) { 
    doc.get_value(idx).erase(idx_to_erase); 
}

} // namespace qbuem::rust
