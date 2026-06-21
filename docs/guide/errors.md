# Error Handling

qbuem-json gives you full control over how errors are handled. You can choose between **throwing exceptions** for known schemas, **monadic chaining** for untrusted data, or **explicit boolean checks** for fine-grained control.

## 🗺️ Choosing Your Strategy

| Strategy | API | Throws? | Best For |
| :--- | :--- | :---: | :--- |
| **Exception-based** | `as<T>()` | ✅ Yes | Known schemas, RPC calls, internal APIs |
| **Monadic/Safe** | `.get("key")` | ❌ No | API responses, config files, user input |
| **Boolean Check** | `is_int()`, `is_valid()` | ❌ No | Fine-grained validation, diagnostics |
| **Pipe Fallback** | `\| default` | ❌ No | Optional fields with sensible defaults |

---

## 💥 Strategy 1: Exception-Based (Strict)

The `as<T>()` API throws a `std::runtime_error` if the key is missing or the type doesn't match. Use it when you trust your data.

```cpp
try {
    qbuem::Document doc;
    auto root = qbuem::parse(doc, R"({"user": {"id": 42, "name": "Alice"}})");

    // Throws if "user" or "id" is missing, or "id" is not an int
    int id = root["user"]["id"].as<int>();
    std::string name = root["user"]["name"].as<std::string>();

    std::cout << "User #" << id << ": " << name << "\n";

} catch (const std::runtime_error& e) {
    std::cerr << "JSON error: " << e.what() << "\n";
}
```

### Parse Errors

A parse failure throws **`qbuem::parse_error`**. It derives from
`std::runtime_error` — so existing `catch (const std::runtime_error&)` /
`catch (const std::exception&)` blocks keep working — and exposes the full
failure location: **`offset()`** (0-based byte position), plus **`line()`** and
**`column()`** (1-based). `what()` reads `"<reason> at line L column C (byte
offset N)"`.

```cpp
try {
    qbuem::Document doc;
    auto root = qbuem::parse(doc, "[1, 2,"); // truncated → throws
} catch (const qbuem::parse_error& e) {
    std::cerr << e.what() << '\n';        // "Invalid JSON at line 1 column 7 (byte offset 6)"
    e.line();    // 1
    e.column();  // 7
    e.offset();  // 6
}
```

`parse_error` is thrown by every parsing entry point — `parse`, `parse_reuse`,
`parse_strict`, and the owning `read<T>()` / `fuse<T>()`. On the relaxed scalar
path `offset()` is the exact failure cursor; on the AVX-512 staged path it is the
offset just past the last fully consumed value. Line/column are computed only on
the cold error path (a single pass over the prefix), so the happy path pays
nothing; for binary inputs (CBOR) `line()`/`column()` are `0`.

### Pretty errors with `format_error`

For human-facing output, **`qbuem::format_error(e, source)`** renders the failing
line with a caret under the column (the same buffer you parsed must still be
alive). It falls back to `e.what()` when no location is available.

```cpp
std::string json = R"({
  "name": "hero",
  "score": 3.5.7,
  "ok": true
})";
try {
    qbuem::read<Config>(json);
} catch (const qbuem::parse_error& e) {
    std::cerr << qbuem::format_error(e, json) << '\n';
}
```

```text
Invalid JSON at line 3 column 15 (byte offset 34)
    "score": 3.5.7,
                ^
```

Very long (e.g. minified) lines are windowed around the caret with a leading `…`
so the message stays readable.

---

## 🛡️ Strategy 2: Monadic / Safe Chain (No Throw)

The `.get("key")` API returns a `SafeValue` — a proxy that propagates `absent` state through the entire chain. **It never throws.** This is ideal for deeply nested structures from untrusted sources.

```cpp
qbuem::Document doc;
auto root = qbuem::parse(doc, R"({
    "config": { "server": { "timeout_ms": 5000 } }
})");

// Deep chain — safe even if any key is missing
int timeout = root.get("config").get("server").get("timeout_ms").value_or(3000);
// → 5000 (or 3000 if any key was missing)

// Check if a value exists before using it
auto city = root.get("user").get("address").get("city");
if (city) { // evaluates to bool
    std::cout << "City: " << city.value_or("") << "\n";
}
```

### `value_or(default)` — Safe Extraction

```cpp
std::string mode    = root.get("settings").get("mode").value_or(std::string{"auto"});
int         timeout = root.get("settings").get("timeout").value_or(5000);
bool        debug   = root.get("settings").get("debug").value_or(false);
```

### Pipe Syntax

The `|` operator is shorthand for `.value_or()` on a `SafeValue`:

```cpp
int port = root.get("server").get("port") | 8080; // 8080 if missing
```

### `size()` and `empty()` — Safe Container Inspection

`SafeValue` has its own `.size()` and `.empty()` that never throw — they return `0` / `true` when the value is absent.

```cpp
// ❌ THROWS bad_optional_access when "items" is missing
size_t n = root.get("items")->size();

// ✅ Returns 0 safely — never throws
size_t n = root.get("items").size();
bool   b = root.get("items").empty();
```

Always call `.size()` and `.empty()` directly on the `SafeValue` (without `->`) to stay in the no-throw zone.

---

## 🔍 Strategy 3: Boolean Type Checks (Explicit Validation)

For diagnostic-quality error messages or when you need to distinguish "missing" from "wrong type":

```cpp
auto v = root["user_count"];

if (!v.is_valid()) {
    log_error("'user_count' key is missing from JSON");
} else if (v.is_int()) {
    process_count(v.as<int>());
} else {
    log_error("'user_count' expected int, got: " + std::string(v.type_name()));
}
```

#### Full list of boolean checkers:

```cpp
v.is_valid();    // key exists
v.is_null();     // null
v.is_bool();     // true / false
v.is_int();      // integer number (int64_t, uint64_t, etc.)
v.is_double();   // floating-point number
v.is_number();   // is_int() || is_double()
v.is_string();   // "text"
v.is_array();    // [...]
v.is_object();   // {...}

v.type_name();   // "null", "bool", "int", "double", "string", "array", "object"
```

---

## 🔐 Strategy 4: RFC 8259 Strict Validation

Use `qbuem::parse_strict()` or `qbuem::rfc8259::validate()` when you need to enforce strict JSON compliance (e.g., for security-sensitive input processing). Strict failures also throw `qbuem::parse_error` with full line/column/offset, and `what()` includes a human-readable reason.

```cpp
#include <qbuem_json/qbuem_json.hpp>

// Validate without parsing (just check validity)
try {
    qbuem::rfc8259::validate("[1, 2,]");  // trailing comma → throws
} catch (const qbuem::parse_error& e) {
    std::cerr << e.what()        // "RFC 8259 violation at line 1 column 7 (byte offset 6): trailing comma in array"
              << " @ " << e.offset(); // 6
}

// Validate and parse in one step
try {
    qbuem::Document doc;
    auto root = qbuem::parse_strict(doc, R"({"key": "value"})"); // OK
    auto bad  = qbuem::parse_strict(doc, R"({"a": 01})");        // leading zero → throws
} catch (const qbuem::parse_error& e) {
    std::cerr << "Strict parse failed: " << e.what() << "\n";
}
```

### Strict UTF-8 validation

Beyond JSON *syntax*, strict mode also enforces RFC 8259 §8.1: the input must be
**well-formed UTF-8**. It rejects malformed byte sequences in string content —
overlong encodings (e.g. `C0 AF`, a classic filter-bypass form of `/`), UTF-8
-encoded surrogates (`ED A0 80`), code points beyond U+10FFFF, lone continuation
bytes, and truncated sequences. The **relaxed** parser (`parse` / `parse_reuse`)
stays byte-transparent so `as<std::string_view>()` can return a zero-copy slice;
use strict mode, or [`decoded()`](/guide/parsing#decoded-vs-raw-strings), when
you need a UTF-8 guarantee.

**Inputs rejected by strict mode:**

| Input | Reason |
| :--- | :--- |
| `[1, 2,]` | Trailing comma |
| `{"a": 01}` | Leading zero |
| `{"key": 'val'}` | Single-quoted string |
| `"…\xC0\xAF…"` | Overlong UTF-8 encoding |
| `"…\xED\xA0\x80…"` | UTF-8-encoded surrogate |
| `"…\x80…"` | Lone UTF-8 continuation byte |
| `[1, 2} garbage` | Trailing garbage |
| `` (empty) | Empty input |

> A lone surrogate written as a `\uXXXX` **escape** (e.g. `"\uD800"`) is *accepted*
> — RFC 8259 leaves this implementation-defined. If you then call `decoded()`,
> the lone surrogate is replaced with U+FFFD so the decoded result is always valid
> UTF-8.

---

## 💡 Strategy Comparison: Real-World Example

Consider parsing a user profile from an API (which may be incomplete):

```cpp
struct UserView {
    std::string name;
    std::string email;
    std::string city;
    int         age;
    bool        is_admin;
};

UserView build_view(std::string_view json) {
    qbuem::Document doc;
    auto root = qbuem::parse(doc, json);

    return UserView {
        // Required fields → throw if missing (programmer error)
        .name  = root["name"].as<std::string>(),
        .email = root["email"].as<std::string>(),

        // Optional fields → safe monadic chain with defaults
        .city     = root.get("address").get("city").value_or(std::string{"Unknown"}),
        .age      = root["age"] | 0,
        .is_admin = root["roles"].get("admin") | false,
    };
}
```
