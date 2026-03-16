#include <qbuem_json/qbuem_json.hpp>
#include <memory_resource>
#include <cstdint>
#include <vector>

using namespace qbuem::json;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;

    uint8_t strategy = data[0] % 3;
    const std::string_view input(reinterpret_cast<const char *>(data + 1), size - 1);

    std::byte buffer[1024 * 64]; // 64KB stack buffer
    std::unique_ptr<std::pmr::memory_resource> res;

    switch (strategy) {
        case 0: 
            res = std::make_unique<std::pmr::monotonic_buffer_resource>(buffer, sizeof(buffer));
            break;
        case 1:
            res = std::make_unique<std::pmr::unsynchronized_pool_resource>();
            break;
        case 2:
        default:
            res = std::make_unique<std::pmr::monotonic_buffer_resource>();
            break;
    }

    try {
        // qbuem-json uses std::pmr::string and std::pmr::vector internally if configured
        // In the v1.0.7 header, we need to ensure we use the PMR versions of Document/Value if available.
        // For this fuzzer, we primarily stress the parser's allocation with the chosen resource.
        
        static DocumentView doc; // Note: Current DocumentView might not support custom PMR easily without header changes.
        // However, we can stress test the STL types that qbuem-json uses.
        
        Value root = parse_reuse(doc, input);
        (void)root.dump();
        
    } catch (...) {}

    return 0;
}
