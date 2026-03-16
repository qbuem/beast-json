#include <qbuem_json/qbuem_json.hpp>
#include <string>
#include <vector>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return 0;
    
    std::string input(reinterpret_cast<const char *>(data), size);
    
    // Specifically target float parsing
    // We try to parse the entire input as a single number or a wrapping object
    try {
        qbuem::Document doc;
        auto view = qbuem::parse(doc, input);
        
        // If it parsed, try to extract floats from everywhere
        if (view.is_number()) {
            (void)view.as<double>();
        } else if (view.is_array()) {
            try {
                for (double d : view.as_array<double>()) {
                    (void)d;
                }
            } catch (...) {}
        }
    } catch (...) {}

    // Direct string to double conversion test (if library exposes it)
    // qbuem_json uses internal eisel_lemire, we trigger it via parse
    
    return 0;
}
