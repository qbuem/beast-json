# fuzz/CMakeLists.txt – libFuzzer targets for qbuem-json
#
# Prerequisites
# ─────────────
#   • Clang (any version with libFuzzer support; tested with 18)
#   • libclang-rt-dev  (Ubuntu: `sudo apt install libclang-rt-18-dev`)
#
# Quick-start
# ───────────
#   cmake -B build-fuzz \
#         -DQBUEM_JSON_BUILD_FUZZ=ON \
#         -DQBUEM_JSON_BUILD_TESTS=OFF \
#         -DQBUEM_JSON_BUILD_BENCHMARKS=OFF \
#         -DCMAKE_CXX_COMPILER=clang++
#   cmake --build build-fuzz
#
#   # Run indefinitely (Ctrl-C to stop; crashes saved as crash-*)
#   ./build-fuzz/fuzz/fuzz_parse    fuzz/corpus/ -max_len=65536
#   ./build-fuzz/fuzz/fuzz_dom     fuzz/corpus/ -max_len=65536
#   ./build-fuzz/fuzz/fuzz_rfc8259  fuzz/corpus/ -max_len=65536
#
#   # Reproduce a specific crash
#   ./build-fuzz/fuzz/fuzz_parse  <crash-file>

# ── Compiler guard ────────────────────────────────────────────────────────────
if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
    message(FATAL_ERROR
        "QBUEM_JSON_BUILD_FUZZ requires Clang (pass -DCMAKE_CXX_COMPILER=clang++). "
        "Detected compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# ── Sanitizer flags ───────────────────────────────────────────────────────────
set(FUZZ_SANITIZERS "fuzzer,address,undefined")

# ── Helper function ───────────────────────────────────────────────────────────
function(add_fuzz_target name)
    set(extra_libs ${ARGN})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE qbuem_json::qbuem_json ${extra_libs})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_compile_options(${name} PRIVATE
        -g
        -O1
        -fsanitize=${FUZZ_SANITIZERS}
        -fno-omit-frame-pointer   # Better stack traces in crash reports
    )
    target_link_options(${name} PRIVATE
        -fsanitize=${FUZZ_SANITIZERS}
    )
endfunction()

# ── Fuzz targets ──────────────────────────────────────────────────────────────
add_fuzz_target(fuzz_parse)      # High-level qbuem::parse() + full accessor surface
add_fuzz_target(fuzz_dom)        # DOM tape parser: qbuem::json::dom::parse_reuse()
add_fuzz_target(fuzz_rfc8259)    # parse_reuse consistency: tape-reuse vs fresh allocation
add_fuzz_target(fuzz_nexus)      # Nexus Fusion: struct mapping stress test
add_fuzz_target(fuzz_roundtrip)  # Semantic consistency: parse-dump-parse
add_fuzz_target(fuzz_api_stress) # DOM mutation and SafeValue stress test
add_fuzz_target(fuzz_float)      # High-precision float parsing (Eisel-Lemire)
add_fuzz_target(fuzz_direct)     # DirectParser & Key mapping stress test
add_fuzz_target(fuzz_diff nlohmann_json::nlohmann_json) # Differential testing vs nlohmann/json
add_fuzz_target(fuzz_pmr)        # PMR allocator stress test
add_fuzz_target(fuzz_patch)      # JSON Pointer & Mutation stress test
