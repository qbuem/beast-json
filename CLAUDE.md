# CLAUDE.md — qbuem-json AI Context

This file provides structured context for AI coding assistants working in this repository.

---

## Language Policy

**All code, comments, documentation, and user-facing strings MUST be written in English.**

Korean or other non-English text in code comments, docs, or strings is a review failure.
Existing Korean comments in legacy files should be translated to English when touched.

---

## Project Overview

**qbuem-json v1.0.7** — Single-header C++20 JSON library.
RFC 8259 compliant, IEEE 754 round-trip, zero external dependencies.

- **Single header**: `include/qbuem_json/qbuem_json.hpp` — ALL logic lives here
- **Standard**: C++20 (`target_compile_features(cxx_std_20)`)
- **No external deps**: header-only INTERFACE library
- **Integration**: `QBUEM_JSON_FIELDS(Struct, field1, field2, ...)` macro + `qbuem::read<T>()` / `qbuem::write()`

---

## Key Files

| File | Purpose |
|------|---------|
| `include/qbuem_json/qbuem_json.hpp` | **The entire library** — single header, all fixes go here |
| `tests/test_serializer.cpp` | Serialization correctness |
| `tests/test_stl_exhaustive.cpp` | STL type coverage |
| `tests/test_dom_roundtrip.cpp` | Parse → serialize round-trip |
| `tests/test_complex_stl.cpp` | Nested/complex types |
| `tests/test_errors.cpp` | Error handling |
| `tests/repro_bugs.cpp` | Bug reproduction cases — add new repro here first |

---

## Build & Test

```bash
# Configure (from repo root)
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release \
      -DQBUEM_JSON_BUILD_TESTS=ON

# Build
cmake --build build --parallel

# Run all tests
cd build && ctest --output-on-failure

# Run specific test
./build/tests/test_serializer

# With AddressSanitizer (recommended before commit)
cmake -G Ninja -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DQBUEM_JSON_BUILD_TESTS=ON \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan && cd build-asan && ctest --output-on-failure
```

---

## Fix Workflow

When a JSON serialization or deserialization produces wrong values:

```
1. REPRODUCE in tests/repro_bugs.cpp
   Add a minimal failing test case that demonstrates the wrong value.

2. FIX in include/qbuem_json/qbuem_json.hpp
   This is the ONLY file to edit for logic fixes.

3. VERIFY
   cmake --build build && cd build && ctest --output-on-failure
   All existing tests must still pass. The repro case must now pass.

4. ALSO run sanitizers
   cmake --build build-asan && cd build-asan && ctest --output-on-failure

5. COMMIT & PUSH
   Commit message format: "fix: <description of the wrong value and correction>"
   Include the repro test case in the commit.
```

> **When working in a project that uses qbuem-json and you discover a library bug:**
> Fix it directly in this repo (qbuem-json). Follow the workflow above — reproduce,
> fix the single header, verify all tests pass, then commit and push here.
> Do NOT work around library bugs in the consuming project.

---

## Common API

```cpp
#include <qbuem_json/qbuem_json.hpp>

// 1. Register struct fields
struct Child {
    int64_t id;
    std::string name;
    std::string birth_date;
};
QBUEM_JSON_FIELDS(Child, id, name, birth_date)

// 2. Serialize → JSON string
std::string json = qbuem::write(child);

// 3. Deserialize ← JSON string (returns Result<T>)
auto result = qbuem::read<Child>(json_str);
if (!result) {
    // result.error() contains parse error info
}
Child c = *result;

// 4. Parse a byte-span body
auto body_sv = std::string_view{
    reinterpret_cast<const char*>(body.data()), body.size()};
auto child_r = qbuem::read<Child>(body_sv);
```

---

## Coding Rules

- **Never add external dependencies** — the library must remain header-only with zero deps.
- **C++20 only** — do not use C++23-only features; this library targets C++20 consumers.
- **All fixes in the single header** — do not split into multiple files.
- **Add a repro test before fixing** — `tests/repro_bugs.cpp` is the bug regression suite.
- **Sanitizer clean** — every fix must pass ASan + UBSan before commit.
