# Debugging Guide

This guide explains exactly how Beast JSON reads and stores data at the core level, then shows you how to diagnose every category of error — from beginner mistakes to expert-level memory issues.

---

## Part 1 — How the Core Works

Understanding the data flow is the foundation of effective debugging. Every parse goes through the same deterministic pipeline.

### The Full Pipeline

```
 Your JSON string (caller-owned buffer — never copied)
        │
        ▼
 ┌──────────────────────────────────────────────────────┐
 │  Stage 1 — SIMD Structural Indexing                  │
 │  • Load 64 bytes/cycle into zmm0 (AVX-512)           │
 │  • 8× VPCMPEQB → 64-bit structural_mask             │
 │  • PCLMULQDQ prefix-XOR → suppress chars in strings  │
 │  • Result: sparse bitset of structural positions      │
 └──────────────────────────────────────────────────────┘
        │  (clean_structural_mask)
        ▼
 ┌──────────────────────────────────────────────────────┐
 │  Stage 2 — Tape Generation (scalar)                  │
 │  • TZCNT iterate only structural positions (5-15%)   │
 │  • Each position → one TapeNode written              │
 │  • Strings → zero-copy string_view into input buf    │
 │  • Numbers → Russ Cox inline value (no strtod)       │
 │  • Jump patches resolved at OBJ_END / ARR_END         │
 └──────────────────────────────────────────────────────┘
        │  (TapeArena filled — 1 malloc total)
        ▼
 ┌──────────────────────────────────────────────────────┐
 │  DocumentView                                        │
 │  • TapeArena*  — owns all TapeNodes                  │
 │  • string_view — points into your input buffer       │
 │  • Stage1Index — reused across calls, no extra alloc │
 └──────────────────────────────────────────────────────┘
        │
        ▼
  beast::Value  { doc*, idx }   — 16-byte lazy handle
```

**Where errors can occur:**

| Stage | Typical errors |
|:---|:---|
| Stage 1 (SIMD) | Unterminated strings, malformed escape sequences |
| Stage 2 (scalar) | Unexpected tokens, number overflow, missing closing bracket |
| `.as<T>()` call | Type mismatch, null value access |
| After parse | Dangling `string_view`, `Document` destroyed too early |

---

## Part 2 — How Data is Stored

Every single element — object, array, string, integer, float, bool, null — occupies exactly **8 bytes** in the TapeArena. There are no pointers to external heap objects.

### The TapeNode: 64-bit Encoding

```
 Bit 63                  Bit 0
  │                        │
  ▼                        ▼
 [63──56][55────────────────0]
   type     payload (56 bits)
   tag
   (8 bits)
```

Click any node below to see its exact encoding:

<TapeInspector />

### Type Tag Reference

| Tag | Name | Payload meaning | Access |
|:---|:---|:---|:---|
| `0x01` | `OBJ_START` | Index of matching `OBJ_END` | `is_object()` |
| `0x02` | `OBJ_END` | Index of matching `OBJ_START` | internal |
| `0x03` | `ARR_START` | Index of matching `ARR_END` | `is_array()` |
| `0x04` | `ARR_END` | Index of matching `ARR_START` | internal |
| `0x05` | `KEY` | 48-bit ptr + 8-bit len into input buf | key matching |
| `0x06` | `STRING` | 48-bit ptr + 8-bit len into input buf | `.as<string_view>()` |
| `0x07` | `UINT64` | Unsigned value inline (≤ 2⁵⁶−1) | `.as<uint64_t>()` |
| `0x08` | `INT64` | Signed value inline (sign-extended) | `.as<int64_t>()` |
| `0x09` | `DOUBLE` | IEEE 754 bit-cast, no `strtod` | `.as<double>()` |
| `0x0A` | `BOOL_TRUE` | Unused | `.as<bool>()` → true |
| `0x0B` | `BOOL_FALSE` | Unused | `.as<bool>()` → false |
| `0x0C` | `NULL_VAL` | Unused | `.is_null()` |

### String Storage: Zero-Copy

The most important thing to understand about Beast JSON strings:

```
 Input buffer (your memory — stack, recv buffer, file mmap):
 ┌──────────────────────────────────────────────────────┐
 │  {  "  n  a  m  e  "  :  "  A  l  i  c  e  "  }    │
 │  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16   │
 └──────────────────────────────────────────────────────┘
         │              │    │
         │  KEY node    │    │  STRING node
         │  tape[1]     │    │  tape[2]
         └──────────────┘    └────────────┐
                                          │
 TapeArena:                               │
 ┌─────────┬───────────────────────────── │ ─────────────┐
 │ tape[1] │ 0x05 · ptr=&buf[2] · len=4  ─┘    KEY "name"│
 │ tape[2] │ 0x06 · ptr=&buf[9] · len=5  ──── STRING "Alice"
 └─────────┴────────────────────────────────────────────┘

 beast::Value → .as<string_view>() → reads tape[2] → {&buf[9], 5}
                                                        ↑
                              Points into YOUR buffer — zero bytes copied
```

**Lifetime rule:** A `string_view` from Beast JSON is valid only while **both** the `Document` and the input buffer are alive. The moment either is destroyed, every `string_view` from that parse becomes a dangling pointer.

### Integer Storage: Fully Inline

```cpp
// How 42 is stored in tape[2] as INT64:
uint64_t raw = (uint64_t(0x08) << 56) | uint64_t(42);
//             ↑ type tag               ↑ value

// How .as<int64_t>() recovers it:
int64_t value = static_cast<int64_t>(raw << 8) >> 8;  // sign-extend 56 bits
// Result: 42  — zero allocations, zero heap access
```

### Jump Indices: O(1) Skip

```
 { "a": [1, 2, 3], "b": true }
  ↓
 tape[0] OBJ_START  jump→9   ─────────────────────────────┐
 tape[1] KEY        "a"                                   │
 tape[2] ARR_START  jump→6   ──────────────┐              │
 tape[3] UINT64     1                      │              │
 tape[4] UINT64     2                      │              │
 tape[5] UINT64     3                      │              │
 tape[6] ARR_END    jump→2   ←─────────────┘              │
 tape[7] KEY        "b"                                   │
 tape[8] BOOL_TRUE                                        │
 tape[9] OBJ_END    jump→0   ←────────────────────────────┘

 To skip the array and find "b":
   tape[tape[2].jump] = tape[6] = ARR_END  → next key at tape[7]
   One array read. O(1) regardless of array size.
```

---

## Part 3 — Error Taxonomy

Explore every error category interactively — from what causes it to how to fix it:

<ParseErrorMap />

---

## Part 4 — Debugging by Level

### 🟢 Beginner: Catching and Reading Errors

**All Beast JSON errors derive from `beast::json::error`.**

```cpp
#include <beast/json.hpp>

try {
    auto doc  = beast::parse(input);
    auto name = doc.root()["name"].as<std::string_view>();
    auto age  = doc.root()["age"].as<int>();
}
catch (const beast::parse_error& e) {
    // Malformed JSON — bad input from network, file, etc.
    std::cerr << "Parse failed: " << e.what()   << "\n";
    std::cerr << "At byte:      " << e.position() << "\n";
    std::cerr << "Context:      " << e.context()  << "\n";
    // e.context() returns up to 20 bytes around the error site
}
catch (const beast::type_error& e) {
    // Wrong .as<T>() call
    std::cerr << "Type error: " << e.what() << "\n";
}
catch (const beast::access_error& e) {
    // Key not found, index out of range
    std::cerr << "Access error: " << e.what() << "\n";
}
catch (const beast::json::error& e) {
    // Catch-all for any Beast JSON error
    std::cerr << "JSON error: " << e.what() << "\n";
}
```

**The three questions to ask when an error occurs:**

1. **What byte position?** — `e.position()` tells you exactly where in the input
2. **What is the input around that position?** — `e.context()` shows the surrounding bytes
3. **What type tag did the node have?** — a `type_error` tells you what tag was found vs. what was expected

---

### 🟢 Beginner: Type-Safe Access Pattern

Always check before you extract:

```cpp
auto doc  = beast::parse(input);
auto root = doc.root();

// ❌ Fragile — throws if key missing or wrong type
auto name = root["name"].as<std::string_view>();

// ✅ Safe — explicit checks
if (!root.is_object()) {
    return error("expected JSON object");
}

auto name_val = root.find("name");
if (!name_val || !name_val->is_string()) {
    return error("missing or non-string 'name' field");
}
auto name = name_val->as<std::string_view>();

// ✅ Also safe — use the 'or_default' variant
auto count = root["count"].as<int>(/*default=*/ 0);
```

---

### 🟡 Intermediate: Inspecting Error Context

When you get a `parse_error`, the context string is your best diagnostic tool:

```cpp
catch (const beast::parse_error& e) {
    auto pos = e.position();      // byte offset into input
    auto ctx = e.context();       // up to 20 bytes around the error

    // Print a visual position marker:
    std::cerr << "Input:   " << std::string(ctx) << "\n";
    std::cerr << "         " << std::string(pos % 20, ' ') << "^\n";
    std::cerr << "Message: " << e.what() << "\n";
}
```

**Reading a `type_error` message:**

```
beast::type_error: expected INT64/UINT64, got STRING at tape[2]
                   ─────────────────────  ─────────────────────
                   what you called as<>() with  what the tape actually contains
```

Check the TapeNode type tags table above to decode the tag name.

---

### 🟡 Intermediate: Validating Before Parsing

For hot paths receiving untrusted input (network, files), validate cheaply before a full parse:

```cpp
// Quick structural check — does not allocate
bool looks_valid = beast::validate(input);

// Check document size (prevent tape exhaustion DoS)
if (input.size() > MAX_DOCUMENT_BYTES) {
    return reject("document too large");
}

// Only parse if validation passes
if (looks_valid) {
    auto root = beast::parse(doc, input);
    // ...
}
```

---

### 🟡 Intermediate: Lifetime Safety Patterns

The most common source of hard-to-debug crashes in Beast JSON code is lifetime violations:

```cpp
// ❌ WRONG — string_view outlives the input
std::string_view get_name(std::string_view json_text) {
    beast::Document doc;
    auto root = beast::parse(doc, json_text);
    return root["name"].as<std::string_view>();
    // ^ This string_view points into json_text.
    //   If json_text was a temporary std::string, it's now dangling.
}

// ✅ CORRECT — return owned data
std::string get_name(std::string_view json_text) {
    beast::Document doc;
    auto root = beast::parse(doc, json_text);
    auto sv   = root["name"].as<std::string_view>();
    return std::string(sv);  // copy out before return
}

// ✅ CORRECT — keep everything alive together
struct ParseResult {
    std::string      input;     // keeps the buffer alive
    beast::Document  doc;       // keeps the tape alive
    beast::Value     root;      // safe to use
};

ParseResult parse_and_keep(std::string json) {
    ParseResult r;
    r.input = std::move(json);
    r.root  = beast::parse(r.doc, r.input);
    return r;  // everything lives together — safe
}
```

**Rule of thumb:** If you need a `string_view` or `Value` to outlive the local scope, either copy it to `std::string` or move the `Document` and input buffer into the same struct.

---

### 🔴 Expert: Dumping the Tape

When you have a hard-to-reproduce parse bug, dump the tape directly to understand what the parser saw:

```cpp
#include <beast/json/debug.hpp>  // debug utilities

beast::Document doc;
auto root = beast::parse(doc, input);

// Print the entire tape as human-readable text
beast::debug::dump_tape(doc, std::cerr);
```

Sample output:

```
TapeArena — 6 nodes, capacity 256
────────────────────────────────────────────────────────
tape[0]  OBJ_START  tag=0x01  jump→5
tape[1]  KEY        tag=0x05  ptr=0x7ffe1234  len=2  → "id"
tape[2]  INT64      tag=0x08  value=42
tape[3]  KEY        tag=0x05  ptr=0x7ffe1238  len=6  → "active"
tape[4]  BOOL_TRUE  tag=0x0A
tape[5]  OBJ_END    tag=0x02  jump→0
────────────────────────────────────────────────────────
Input buffer: 0x7ffe1230  length: 26
Stage1Index:  24 structural positions
```

**What to look for in the dump:**

| Symptom | Cause |
|:---|:---|
| Node count much lower than expected | Parser stopped early — check for errors |
| KEY node count ≠ STRING node count | Malformed object (missing value for a key) |
| Jump index points to wrong node | Nested depth mismatch — mismatched braces |
| STRING ptr very different from KEY ptr | Input buffer layout issue |
| `DOUBLE` where you expected `INT64` | Number has a decimal point or exponent in JSON |

---

### 🔴 Expert: AddressSanitizer for Lifetime Bugs

Beast JSON's zero-copy design means lifetime bugs produce **no exception** — only silent UB that may or may not crash. AddressSanitizer catches them reliably:

```bash
# Build with ASan
clang++ -fsanitize=address,undefined -g -O1 your_code.cpp -o app

# Run — ASan reports use-after-free on dangling string_view
./app
```

A dangling `string_view` from Beast JSON produces an ASan report like:

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
READ of size 5 at 0x... thread T0
    #0 std::string_view::operator[] src/your_code.cpp:42
    #1 your_function               src/your_code.cpp:42

0x... is located inside of 26-byte region [0x..., 0x...)
freed by thread T0 here:
    #0 operator delete[]           <system>
    #1 std::string::~string        src/your_code.cpp:38
```

The line numbers in the `freed by` section show exactly where the input buffer was destroyed. Compare it with the `READ of size` location to find the mismatch.

---

### 🔴 Expert: SIMD Edge Cases

A small number of inputs trigger non-obvious Stage 1 behaviour. These are **not bugs** — they are correctness invariants that every advanced user should know:

#### Very long strings (>= 2³² bytes)

The 56-bit payload holds a 48-bit virtual address and an 8-bit length hint. For strings longer than 255 bytes, the 8-bit length overflows into a sentinel value and Stage 2 falls back to a scalar re-scan to compute the true length. Parse correctness is preserved; Stage 1 throughput is unchanged, but Stage 2 has a small additional cost for that specific string token.

#### NEON path on ARM: no `VCOMPRESSB`

ARM NEON lacks the `VCOMPRESSB` instruction. On ARM64, Beast JSON uses a `VBSL`-based gather instead. The structural iteration in Stage 2 uses a slightly longer scalar loop. Throughput on ARM is approximately 40-60% of the AVX-512 path, which is still 20–30× faster than a naive scalar parser.

#### Escaped quotes near the 64-byte boundary

```
// Input crossing a 64-byte window boundary:
// window N ends at:       ...\"
// window N+1 starts at:   value"...
```

The prefix-XOR carry value is passed between windows. If an escaped quote (`\"`) sits at the boundary, the carry must correctly suppress the `"` in window N+1. Beast JSON propagates the carry via a thread-local scalar variable. If you find a parse bug that only appears on inputs whose escaped-quote positions are multiples of 64 bytes, this is the area to investigate.

---

## Part 5 — Quick Diagnostic Checklist

Use this when an error occurs and you're not sure where to start:

### Parse errors

- [ ] Is the JSON syntactically valid? Test with `jq . < input.json` or an online validator.
- [ ] Does the input have trailing commas? (JSON disallows them; JavaScript allows them.)
- [ ] Are all strings closed with a matching `"`?
- [ ] Are all `{` matched with `}` and `[` matched with `]`?
- [ ] Are escape sequences valid? (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, `\uXXXX` only)
- [ ] Does any integer exceed 2⁵⁶ − 1? Use double or string instead.
- [ ] Is the input null-terminated, or is the length passed correctly?

### Type errors

- [ ] What does `dump_tape()` show as the actual type tag?
- [ ] Did you call `.as<int>()` on a float like `1.0`? Use `.as<double>()` instead.
- [ ] Did you call `.as<string_view>()` on a key that might not exist? Use `.find()` first.
- [ ] Are array indices zero-based and within range?

### Lifetime / crash

- [ ] Is the input buffer still alive when you access `string_view` values?
- [ ] Is the `Document` still alive when you use any `Value` from it?
- [ ] Did you return a `Value` or `string_view` from a function where the `Document` was local?
- [ ] Did you move the `Document` after parsing (moving invalidates all `Value` handles)?
- [ ] Run with `-fsanitize=address` — is there a use-after-free report?

### Serialization errors

- [ ] Are you passing a fixed-size `char[]` buffer? Use `std::string` with `reserve()` instead.
- [ ] Is the `write_to` return value checked for overflow errors?
- [ ] Are custom serialization macros (`BEAST_SERIALIZE`) matching the struct fields exactly?

---

## Summary

```
  Data flows in one direction:

  Your JSON bytes  →  Stage 1 (SIMD)  →  Stage 2 (scalar)  →  TapeArena
                                                                    │
                                                                    ▼
                                                           beast::Value (lazy handle)
                                                                    │
                                                           .as<T>() on demand
                                                                    │
                                                                    ▼
                                                           Typed C++ value
                                                           (zero allocation)

  Your input buffer must stay alive for the entire arrow above.
  The TapeArena is owned by Document and lives as long as it does.
  Values are 16-byte handles — safe to copy, dangerous to outlive their Document.
```

If in doubt: **copy `string_view` to `std::string`** before the input or `Document` goes out of scope. Every other performance characteristic of Beast JSON survives this one safety net.
