#include <qbuem_json/qbuem_json.hpp>
#include <cstdint>
#include <string_view>
#include <string>

using namespace qbuem::json;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 10) return 0;
    
    // Split input into "base doc" and "operations"
    size_t mid = size / 2;
    const std::string_view base_input(reinterpret_cast<const char *>(data), mid);
    const std::string_view ops_input(reinterpret_cast<const char *>(data + mid), size - mid);

    static DocumentView doc;
    try {
        Value root = parse_reuse(doc, base_input);
        
        // Exercise JSON Pointer aggressively
        std::string pointer(ops_input);
        if (!pointer.empty() && pointer[0] != '/') pointer = "/" + pointer;
        
        try {
            (void)root.at(pointer);
        } catch (...) {}

        // Exercise mutations (patch-like)
        if (root.is_object()) {
            root.insert("fuzz_key", std::string(ops_input));
            (void)root["fuzz_key"].is_valid();
            root.erase("fuzz_key");
        }

        (void)root.dump();
    } catch (...) {}

    return 0;
}
