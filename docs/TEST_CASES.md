# Beast JSON — Test Cases (TC) Specification

> This document provides complete transparency regarding the Test Cases (TC) engineered to validate the Beast JSON 1.0 architecture. It allows open-source contributors and security researchers to understand our testing coverage and verification philosophy.

[TOC]

## 1. Overview of the Verification Strategy

Beast JSON is tested maliciously. The core philosophy of our test suites is **Pessimistic Verification**: we assume that every JSON payload from the network is actively trying to crash the parser, overflow the stack, or trigger undefined behavior.

Our continuous integration (CI) tests execute over **60+ rigorous behavioral test blocks** covering fundamental RFC compliance, extreme edge-case boundaries, memory-safety under fuzzing, and API usability.

All tests utilize GoogleTest and are physically located in the `tests/` directory.

---

## 2. Core Compliance & Standard Verification
These tests guarantee that Beast JSON flawlessly adheres to the official JSON Standard (RFC 8259) and handles raw data bytes correctly.

### 2.1 RFC 8259 Compliance (`test_compliance.cpp`)
- **Objective**: Ensure absolute compliance with the official JSON specification.
- **Valid Cases**: 
  - Standard scalar parsing (Booleans, Null, Strings, Floats, Integers).
  - Exponential notation parsing (`1.23e+456`).
  - Empty Array `[]` and Empty Object `{}` boundary conditions.
- **Rejection Cases (Must Fail)**:
  - Trailing commas in arrays/objects (Strict mode).
  - Unquoted keys or single-quoted strings.
  - Naked decimal points (`.5`, `1.`) or leading plus signs (`+1`).
  - Bare commas (`[,]`).
- **Implementation-Defined (Limit Testing)**:
  - Extreme deep nesting (e.g., Arrays nested 512 levels deep) to test stack pressure.
  - Subnormal float parsing (e.g., `4.9406564584124654e-324`).
  - Naked/Lone UTC surrogate halves (`\uD83D`) isolated in strings.

### 2.2 Strict Number Format (`test_strict_number.cpp`)
- **Objective**: Guarantee that invalid number formatting does not bleed into undefined states.
- **Test Scenarios**:
  - Rejection of leading zeros (`01`, `-007`).
  - Validation of fractional components.
  - Boundary checking at the maximum limits of `int64_t` and `uint64_t`.

### 2.3 UTF-8 and Unicode Safety (`test_unicode.cpp`, `test_utf8_validation.cpp`)
- **Objective**: Verify that multi-byte characters and escaped Unicodes are decoded safely and exactly without cross-buffer bleeding.
- **Test Scenarios**:
  - Valid multi-byte parsing (Korean, Japanese, Emoji surrogate pairs).
  - Rejection of truncated or invalid UTF-8 byte sequences.
  - Rejection of overlong UTF-8 encodings.

### 2.4 Unescaped Control Characters (`test_control_char.cpp`)
- **Objective**: Defend against malicious injection of raw control structures.
- **Test Scenarios**:
  - Strict rejection of unescaped `\n`, `\t`, `\r` inside strings.
  - Rejection of raw `\0` (Null Bytes) injected inside JSON values.

---

## 3. DOM & Abstract API Verification
These tests verify the accuracy of the C++ API representation, ensuring that when developers request data, they receive the exact physical data.

### 3.1 Lazy Type Inference (`test_lazy_types.cpp`, `test_lazy_roundtrip.cpp`)
- **Objective**: Prevent type-punned data corruption when casting Tape data to C++ variables.
- **Test Scenarios**:
  - Implicit casting limits (e.g., extracting an integer node as a double).
  - Ensuring JSON strings do not implicitly cast to booleans/numbers.
  - Data integrity after a full cycle: `Parse -> Memory -> Serialize -> Compare String`.

### 3.2 Value Accessors (`test_value_accessors.cpp`)
- **Objective**: Ensure the monadic `SafeValue` interface operates dynamically.
- **Test Scenarios**:
  - Validating `.as_string()`, `.as_int()`, `.as_bool()` throws on failure.
  - Validating `.get("key").value_or(10)` falls back gracefully when a key doesn't exist.
  - Nested boundary testing: Missing intermediate array indices do not cause segfaults.

---

## 4. Extensions & Syntactic Sugar
Test suites validating functionality operating outside the rigid RFC 8259 boundary, meant for developer ergonomics.

### 4.1 Relaxed Syntax Parsing (`test_relaxed.cpp`)
- **Objective**: Validate the engine's ability to optionally parse human-friendly configuration files.
- **Test Scenarios**:
  - Parsing single-line `//` and multi-line `/* */` comments (`test_comments.cpp`).
  - Permitting trailing commas without validation throwing (`test_trailing_commas.cpp`).
  - Allowing bare/unquoted object keys (Similar to JavaScript).

### 4.2 Duplicate Keys Strategy (`test_duplicate_keys.cpp`)
- **Objective**: Define behavior when an object receives multiple identical keys.
- **Test Scenarios**:
  - Confirming the Last-Write-Wins (LWW) architecture operates correctly on tape traversal.

### 4.3 Serializer Engine (`test_serializer.cpp`)
- **Objective**: Verify that C++ objects are efficiently dumped down to raw character arrays.
- **Test Scenarios**:
  - Verifying characters requiring escapes (like `\n` or `"`) correctly expand.
  - Formatting arrays flawlessly without lingering trailing spaces.

---

## 5. Defense-in-Depth (Fuzzing)
Beyond unit tests, Beast JSON uses continuous randomized data injection (libFuzzer) to achieve mathematically proven memory safety.

### 5.1 libFuzzer Suites (`fuzz/fuzz_*.cpp`)
- **Target**: Ensure `0` Heap Buffer Overflows, `0` Stack Underflows, and `0` Memory Leaks across billions of randomized iterations.
- **Methodology**:
  - `fuzz_parse`: Injects raw mutated bytes into the parser core.
  - `fuzz_lazy`: Performs randomized `insert()`, `erase()`, and `push_back()` DOM mutations onto corrupted structures.
  - Complied dynamically with Clang AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan).

---

## 6. How to Run the Tests Locally

If you are contributing code, you must execute the entire test suite before submitting a Pull Request.

```bash
# Generate build configuration enabling Tests
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBEAST_JSON_BUILD_TESTS=ON

# Compile the tests
cmake --build build -j10

# Run the GoogleTest harness
ctest --test-dir build --output-on-failure
```

> A successful local execution will output: `[  PASSED  ] 61 tests.` (Numbers may vary based on added edge-cases).
