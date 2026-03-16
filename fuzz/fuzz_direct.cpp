#include <qbuem_json/qbuem_json.hpp>
#include <string>
#include <cstdint>

struct DirectTarget {
    int id;
    std::string type;
};
QBUEM_JSON_FIELDS(DirectTarget, id, type)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;
    
    std::string_view input(reinterpret_cast<const char *>(data), size);
    
    // Test Direct Parsing (Nexus Pulse)
    try {
        // This triggers DirectParser and key parsing logic
        auto obj = qbuem::fuse<DirectTarget>(input);
        (void)obj.id;
    } catch (...) {}

    // Test with variations
    try {
        (void)qbuem::fuse_strict<DirectTarget>(input);
    } catch (...) {}

    return 0;
}
