#include <qbuem_json/qbuem_json.hpp>
#include <cstdint>
#include <string_view>
#include <vector>

using namespace qbuem::json;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;

    static DocumentView doc;
    // Use part of the input to drive random API calls
    uint32_t seed;
    std::memcpy(&seed, data, 4);
    const std::string_view input(reinterpret_cast<const char *>(data + 4), size - 4);

    try {
        Value root = parse_reuse(doc, input);

        // Mix multiple operations based on bits of the seed
        for (int i = 0; i < 8; ++i) {
            uint8_t op = (seed >> (i * 4)) & 0xF;

            if (root.is_object()) {
                switch (op % 4) {
                    case 0: root.insert("key_" + std::to_string(i), i); break;
                    case 1: root.erase("key_" + std::to_string(i)); break;
                    case 2: (void)root["key_" + std::to_string(i)].is_valid(); break;
                    case 3: root["key_" + std::to_string(i)] = "val_" + std::to_string(i); break;
                }
            }

            if (root.is_array()) {
                switch (op % 3) {
                    case 0: root.push_back(double(i)); break;
                    case 1: if (root.size() > 0) root.erase(0); break;
                    case 2: (void)root[0].is_valid(); break;
                }
            }

            // Exercise SafeValue paths
            (void)root.get("a")["b"][i].value_or(99);
            
            // Exercise JSON Pointer
            try { (void)root.at("/" + std::to_string(i)); } catch (...) {}
        }

        // Final serialization check
        (void)root.dump();

    } catch (...) {
        // Broad catch to allow fuzzer to continue on expected type errors
    }

    return 0;
}
