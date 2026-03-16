#include <qbuem_json/qbuem_json.hpp>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string_view>
#include <string>

using namespace qbuem::json;
using nlohmann_json = nlohmann::json;

// Check semantic equality between qbuem::Value and nlohmann::json
static bool compare(const Value &q, const nlohmann_json &n) {
    if (q.is_null()) return n.is_null();
    if (q.is_bool()) return n.is_boolean() && (q.as<bool>() == n.get<bool>());
    if (q.is_int()) {
        try { return (n.is_number_integer() || n.is_number_unsigned()) && (q.as<int64_t>() == n.get<int64_t>()); }
        catch (...) { return false; }
    }
    if (q.is_double()) {
        try { return n.is_number() && (std::abs(q.as<double>() - n.get<double>()) < 1e-10); }
        catch (...) { return false; }
    }
    if (q.is_string()) return n.is_string() && (q.as<std::string_view>() == n.get<std::string>());

    if (q.is_array()) {
        if (!n.is_array() || q.size() != n.size()) return false;
        size_t i = 0;
        for (auto elem : q.elements()) {
            if (!compare(elem, n[i++])) return false;
        }
        return true;
    }

    if (q.is_object()) {
        if (!n.is_object() || q.size() != n.size()) return false;
        for (auto [k, v] : q.items()) {
            std::string key(k);
            if (!n.contains(key) || !compare(v, n[key])) return false;
        }
        return true;
    }

    return false;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const std::string_view input(reinterpret_cast<const char *>(data), size);
    
    static DocumentView doc;
    try {
        Value q_root = parse_reuse(doc, input);
        
        try {
            nlohmann_json n_root = nlohmann_json::parse(input);
            if (!compare(q_root, n_root)) {
                __builtin_trap(); // Semantic mismatch
            }
        } catch (const nlohmann_json::parse_error &) {
            // If nlohmann fails but qbuem succeeds, it might be a spec compliance difference
            // or qbuem being more lenient (which is allowed unless in strict mode).
        }
    } catch (...) {}

    return 0;
}
