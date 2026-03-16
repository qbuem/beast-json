#pragma once
#include <qbuem_json/qbuem_json.hpp>
#include <memory>
#include <string>
#include <string_view>
#include "rust/cxx.h"

namespace qbuem::rust {

struct TapeNode; // Forward declaration (cxx provides the definition)

class RustDocument {
public:
    qbuem::Document doc;
    qbuem::Value    root;
    std::string     source;

    void parse(::rust::Str json) {
        source = std::string(json);
        root = qbuem::parse(doc, source);
        if (!root.is_valid()) {
            throw std::runtime_error("qbuem-json: parse error");
        }
    }

    qbuem::Value get_root() const { return root; }
    qbuem::Value get_value(uint32_t idx) const { 
        return qbuem::Value(const_cast<qbuem::Document&>(doc), idx); 
    }
};

std::unique_ptr<RustDocument> create_doc();
void parse(RustDocument &doc, ::rust::Str json);

// Direct accessors
const TapeNode* get_tape_ptr(const RustDocument &doc);
size_t get_tape_size(const RustDocument &doc);
const uint8_t* get_source_ptr(const RustDocument &doc);
size_t get_source_size(const RustDocument &doc);

// Complex ops
::rust::String dump(const RustDocument &doc, uint32_t idx, int32_t indent);
void dump_to(const RustDocument &doc, uint32_t idx, int32_t indent, ::rust::String &out);
uint32_t at_path_idx(const RustDocument &doc, uint32_t idx, ::rust::Str path);

// Mutations
void set_null(RustDocument &doc, uint32_t idx);
void set_bool(RustDocument &doc, uint32_t idx, bool b);
void set_int(RustDocument &doc, uint32_t idx, int64_t i);
void set_double(RustDocument &doc, uint32_t idx, double d);
void set_string(RustDocument &doc, uint32_t idx, ::rust::Str s);

void insert_raw(RustDocument &doc, uint32_t idx, ::rust::Str key, ::rust::Str raw_json);
void erase_key(RustDocument &doc, uint32_t idx, ::rust::Str key);
void erase_idx(RustDocument &doc, uint32_t idx, size_t idx_to_erase);

} // namespace qbuem::rust
