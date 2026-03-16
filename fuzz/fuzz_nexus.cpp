#include <qbuem_json/qbuem_json.hpp>
#include <vector>
#include <string>
#include <optional>
#include <map>
#include <cstdint>

using namespace qbuem::json;

struct NestedFuzz {
    int id;
    std::string name;
};
QBUEM_JSON_FIELDS(NestedFuzz, id, name)

struct FuzzTarget {
    int i;
    double d;
    bool b;
    std::string s;
    std::vector<int> v;
    std::optional<std::string> o;
    std::map<std::string, NestedFuzz> m;
};
QBUEM_JSON_FIELDS(FuzzTarget, i, d, b, s, v, o, m)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const std::string_view input(reinterpret_cast<const char *>(data), size);
    
    // 1. Test Nexus Fusion (Zero-Tape decoding)
    try {
        auto obj = qbuem::fuse<FuzzTarget>(input);
        
        // 2. Test Encoding (FastWriter)
        std::string encoded;
        qbuem::json::detail::FastWriter fw(encoded);
        qbuem_json_append_fw(fw, obj);
        
        // 3. Roundtrip check (Best effort)
        try {
            (void)qbuem::fuse<FuzzTarget>(encoded);
        } catch (...) {}
        
    } catch (const std::exception &) {
        // Expected for malformed JSON
    }
    
    // 4. Test Strict Nexus Fusion
    try {
        (void)qbuem::fuse_strict<FuzzTarget>(input);
    } catch (...) {}

    return 0;
}
