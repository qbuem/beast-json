#include <qbuem_json/qbuem_json.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <cmath>

using namespace qbuem::json;

// Recursive semantic equality check
static bool are_equal(const Value &a, const Value &b) {
    if (a.type_name() != b.type_name()) return false;
    if (!a.is_valid() || !b.is_valid()) return a.is_valid() == b.is_valid();

    if (a.is_null()) return true;
    if (a.is_bool()) return a.as<bool>() == b.as<bool>();
    if (a.is_int()) return a.as<int64_t>() == b.as<int64_t>();
    if (a.is_double()) {
        double va = a.as<double>();
        double vb = b.as<double>();
        if (std::isnan(va)) return std::isnan(vb);
        if (std::isinf(va)) return std::isinf(vb) && (std::signbit(va) == std::signbit(vb));
        return std::abs(va - vb) < 1e-12;
    }
    if (a.is_string()) return a.as<std::string_view>() == b.as<std::string_view>();

    if (a.is_array()) {
        if (a.size() != b.size()) return false;
        auto it_a = a.elements().begin();
        auto it_b = b.elements().begin();
        for (; it_a != a.elements().end(); ++it_a, ++it_b) {
            if (!are_equal(*it_a, *it_b)) return false;
        }
        return true;
    }

    if (a.is_object()) {
        if (a.size() != b.size()) return false;
        for (auto [k, v] : a.items()) {
            if (!are_equal(v, b[k])) return false;
        }
        return true;
    }

    return false;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const std::string_view input(reinterpret_cast<const char *>(data), size);

    static DocumentView doc1;
    static DocumentView doc2;

    try {
        // 1. Parse original
        Value root1 = parse_reuse(doc1, input);
        
        // 2. Dump (Compact)
        std::string dumped = root1.dump();
        
        // 3. Re-parse
        Value root2 = parse_reuse(doc2, dumped);
        
        // 4. Verify semantic equality
        if (!are_equal(root1, root2)) {
            __builtin_trap(); // Signal semantic failure to fuzzer
        }

        // 5. Test Pretty Print Roundtrip
        std::string pretty = root1.dump(2);
        Value root3 = parse_reuse(doc1, pretty); 
        if (!are_equal(root1, root3)) {
            __builtin_trap();
        }

    } catch (const std::exception &) {
        // Expected for invalid JSON
    }

    return 0;
}
