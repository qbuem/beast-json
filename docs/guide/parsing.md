# Parsing & Access

qbuem-json uses a **Tape DOM model** — it parses into a flat tape in a single pass, and you access values on demand. It's designed to be both fast and easy to use.

## 🚀 Quick Start

```cpp
#include <qbuem_json/qbuem_json.hpp>

int main() {
    std::string json = R"({"name": "Alice", "age": 30, "active": true})";

    qbuem::Document doc;
    qbuem::Value root = qbuem::parse(doc, json);

    std::string name = root["name"].as<std::string>(); // Alice
    int         age  = root["age"].as<int>();           // 30
    bool        flag = root["active"].as<bool>();       // true
}
```

## 📦 Parsing Entry Points

### `qbuem::parse(doc, json)` — Standard Parse

The primary API. Takes a `Document` (which owns the tape memory) and a `string_view`.

```cpp
qbuem::Document doc;
qbuem::Value root = qbuem::parse(doc, R"({"key": "value"})");
```

> [!IMPORTANT]
> `Document` must **outlive** all `Value` objects derived from it. Values are lightweight 16-byte handles pointing into the document's tape.

### `qbuem::parse_reuse(doc, json)` — Explicit Reuse for Hot Loops

For high-frequency parsing (e.g., JSON streams), use `qbuem::parse_reuse()` on the same document. It resets the tape and mutation overlays without reallocating heap memory, making the intent self-documenting in hot-loop code.

```cpp
qbuem::Document doc;
doc.reserve(4 * 1024); // optional: pre-warm to avoid first-parse realloc
std::string line;

while (std::getline(socket_stream, line)) {
    // Re-uses the previously allocated tape capacity — zero extra malloc
    qbuem::Value root = qbuem::parse_reuse(doc, line);
    if (!root.is_valid()) continue;
    process(root);
}
```

> [!NOTE]
> `qbuem::parse()` and `qbuem::parse_reuse()` behave identically when called on the same `Document`. `parse_reuse()` is provided as a self-documenting alias that makes reuse intent explicit.

### `qbuem::parse_strict(doc, json)` — RFC 8259 Strict Mode

Throws `qbuem::parse_error` on any RFC 8259 violation — trailing commas, leading zeros, single-quoted strings, trailing garbage — **and** on malformed UTF-8 (overlong encodings, UTF-8-encoded surrogates, code points > U+10FFFF, lone continuation bytes; see [Error Handling](/guide/errors#strict-utf-8-validation)). The relaxed `parse` / `parse_reuse` stay byte-transparent and do not UTF-8-validate.

```cpp
try {
    qbuem::Document doc;
    auto root = qbuem::parse_strict(doc, "[1, 2,]"); // throws!
} catch (const qbuem::parse_error& e) {
    // "RFC 8259 violation at offset 6: trailing comma in array"
    std::cerr << e.what() << " (byte " << e.offset() << ")\n";
}
```

> [!NOTE]
> `parse_error` derives from `std::runtime_error` (so old catch blocks still work)
> and adds `offset()` — the byte position of the failure. It is thrown by every
> parsing entry point: `parse`, `parse_reuse`, `parse_strict`, `read<T>`, `fuse<T>`.

### `qbuem::read<T>(json)` — Deserialize via Tape-DOM

The standard way to map JSON to types. It first builds a Tape (SIMD) and then maps it to your type `T`. Excellent for large files and general purpose use.

```cpp
auto user = qbuem::read<User>(R"({"name": "Alice", "age": 30})");
```

### `qbuem::fuse<T>(json)` — Direct Fusion (Nexus Engine)

The **lowest-latency** way to parse. It bypasses the Tape entirely and streams JSON directly into your struct. Requires your struct to be registered via `QBUEM_JSON_FIELDS`.

```cpp
// 0.0 Tape Allocation. O(1) Perfect Hash Dispatch.
auto user = qbuem::fuse<User>(R"({"name": "Alice", "age": 30})");
```

---

## 🎯 Accessing Values

### Scalar Access with `.as<T>()`

Type-safe access that throws `std::runtime_error` on type mismatch.

```cpp
qbuem::Document doc;
auto root = qbuem::parse(doc, R"({"id": 101, "score": 9.87, "tag": "vip", "active": true})");

int64_t id     = root["id"].as<int64_t>();
double  score  = root["score"].as<double>();
std::string tag = root["tag"].as<std::string>();
bool    active = root["active"].as<bool>();

// Zero-copy string view (valid as long as doc is alive)
std::string_view sv = root["tag"].as<std::string_view>();
```

### `decoded()` vs raw strings

`as<std::string>()` and `as<std::string_view>()` return the **raw on-the-wire
bytes** of the string — escape sequences are **not** decoded. This is deliberate:
the `string_view` form is a zero-copy slice into the document. When you need the
**logical** value with escapes resolved, call **`decoded()`**:

```cpp
auto root = qbuem::parse(doc, R"({"path": "C:\\tmp\\a.txt", "emoji": "😀"})");

std::string_view raw = root["path"].as<std::string_view>(); // C:\\tmp\\a.txt  (raw, zero-copy)
std::string      log = root["path"].decoded();              // C:\tmp\a.txt   (unescaped)
std::string      em  = root["emoji"].decoded();             // 😀  (surrogate pair → UTF-8)
```

`decoded()` processes `\" \\ \/ \b \f \n \r \t` and `\uXXXX` (combining surrogate
pairs into astral code points). A lone/unpaired surrogate becomes **U+FFFD**, so
`decoded()` output is **always valid UTF-8**.

> [!TIP]
> Use the raw `as<std::string_view>()` for zero-copy comparisons and forwarding;
> use `decoded()` whenever the bytes are shown to a user, written to a non-JSON
> sink, or compared against a decoded value.

### Implicit Conversion (Ergonomic Shorthand)

```cpp
// Implicit conversions via operator T()
int         id    = root["id"];     // operator int()
double      score = root["score"];  // operator double()
std::string tag   = root["tag"];    // operator std::string()
bool        flag  = root["active"]; // operator bool()
```

### Non-Throwing Access with `.try_as<T>()`

Returns `std::optional<T>` — never throws.

```cpp
std::optional<int>    id    = root["id"].try_as<int>();
std::optional<double> score = root["score"].try_as<double>();

if (id) std::cout << "ID: " << *id << "\n";
```

### Pipe Fallback `| default_value` — Never Throws

The safest and most ergonomic pattern. Provides a default if the key is missing or the type doesn't match.

```cpp
int      id    = root["id"]      | -1;
double   score = root["score"]   | 0.0;
bool     flag  = root["active"]  | false;
std::string name = root["name"]  | std::string{"anonymous"};
```

### Safe Monadic Chains with `.get()`

Perfect for deeply nested access without checking every level. Never throws.

```cpp
auto root = qbuem::parse(doc, R"({
    "user": { "profile": { "city": "Seoul" } }
})");

// Propagates "absent" state through entire chain
std::string city = root.get("user").get("profile").get("city").value_or("Unknown");
// → "Seoul"

// If any key is missing, returns the default:
std::string zip = root.get("user").get("profile").get("zip").value_or("00000");
// → "00000" (no exception!)
```

---

## 🔍 Type Checkers

All type checkers return `bool` and **never throw**.

```cpp
auto v = root["data"];

v.is_valid();    // false if key doesn't exist
v.is_object();   // {"a": 1}
v.is_array();    // [1, 2, 3]
v.is_string();   // "hello"
v.is_int();      // 42
v.is_double();   // 3.14
v.is_bool();     // true / false
v.is_null();     // null
v.is_number();   // is_int() || is_double()

// Get a human-readable name of the type
std::string_view tname = v.type_name(); // "int", "string", "array", etc.
```

```cpp
// Pattern: check-then-access
if (root["count"].is_int()) {
    std::cout << "Count: " << root["count"].as<int>() << "\n";
}
```

---

## 🔑 Object Access

### Key Lookup with `find()`

Returns `std::optional<Value>`, distinguishing between "key absent" and "key with wrong type".

```cpp
if (auto config = root.find("config")) {
    int timeout = config->value("timeout", 5000);
    std::string mode = config->value("mode", std::string{"fast"});
}
```

### `contains(key)` — Key Existence Check

```cpp
if (root.contains("optional_field")) {
    process(root["optional_field"]);
}
```

### `value(key, default)` — Access with Default

```cpp
// Never throws. Returns default if key absent or wrong type.
int    timeout = root.value("timeout", 5000);
double ratio   = root.value("ratio", 1.0);
std::string mode = root.value("mode", std::string{"default"});
```

### Duplicate keys

RFC 8259 says object names *should* (not *must*) be unique, so duplicates are
valid JSON and both engines accept them. The resolution is **deterministic but
differs by engine**:

| Engine | API | Duplicate resolution |
| :--- | :--- | :--- |
| DOM | `root["k"]`, `read<T>` | **first**-wins (returns the first occurrence) |
| Nexus | `fuse<T>` | **last**-wins (overwrites the field, matching JS/Python/Go) |

If duplicate keys carry security meaning in untrusted input, reject or
canonicalize upstream — do not rely on cross-engine agreement.

---

## 📋 Iterating Collections

### Object Iteration (Key-Value Pairs)

```cpp
auto root = qbuem::parse(doc, R"({"a": 1, "b": 2, "c": 3})");

// Structured bindings (C++20)
for (auto [key, val] : root.items()) {
    std::cout << key << " = " << val.as<int>() << "\n"; // a=1, b=2, c=3
}

// Keys only
for (std::string_view key : root.keys()) { /* ... */ }

// Values only
for (auto val : root.values()) { /* ... */ }
```

### Array Iteration

```cpp
auto root = qbuem::parse(doc, R"({"scores": [95, 87, 100]})");

// Generic element access
for (auto elem : root["scores"].elements()) {
    std::cout << elem.as<int>() << " ";  // 95 87 100
}

// Typed view — zero allocation
for (int score : root["scores"].as_array<int>()) {
    std::cout << score << " ";  // 95 87 100
}
```

### C++20 Ranges

```cpp
#include <ranges>
auto scores = root["scores"].elements();

// Filter + transform pipeline
auto high_scores = scores
    | std::views::filter([](auto v){ return v.as<int>() >= 90; })
    | std::views::transform([](auto v){ return v.as<int>(); });

for (int s : high_scores) std::cout << s << " ";  // 95 100

// Algorithm
auto max_it = std::ranges::max_element(scores, {}, [](auto v){ return v.as<int>(); });
std::cout << "Best: " << max_it->as<int>() << "\n";  // 100
```

---

## 📌 JSON Pointer (RFC 6901)

Access deeply nested values using a path string.

```cpp
auto root = qbuem::parse(doc, R"({
    "store": { "book": [{"title": "C++ Primer"}, {"title": "Effective C++"}] }
})");

// Runtime JSON Pointer
std::string t = root.at("/store/book/0/title").as<std::string>(); // "C++ Primer"

// Compile-time JSON Pointer (zero runtime overhead)
using namespace qbuem::literals;
auto title = root.at("/store/book/1/title"_jptr).as<std::string>(); // "Effective C++"
```

JSON Pointer addresses **one** node. For multi-match queries (wildcards,
recursive descent, slices), use JSONPath below.

---

## 🛣️ JSONPath (RFC 9535)

`qbuem::query(root, path)` returns **all** Values a [JSONPath](https://www.rfc-editor.org/rfc/rfc9535.html)
query selects, in document order:

```cpp
auto root = qbuem::parse(doc, store_json);

qbuem::query(root, "$.store.book[0].title");   // a single title
qbuem::query(root, "$.store.book[-1].author"); // negative index
qbuem::query(root, "$.store.book[*].author");  // every author (wildcard)
qbuem::query(root, "$..price");                // every price anywhere (recursive descent)
qbuem::query(root, "$.nums[1:4]");             // array slice
qbuem::query(root, "$.nums[::-1]");            // reversed
qbuem::query(root, "$.nums[0, 2, 4]");         // union of indices
qbuem::query(root, "$['store']['bicycle']");   // bracket + quoted names

for (const qbuem::Value& v : qbuem::query(root, "$.store.book[*].title"))
    std::cout << v.as<std::string_view>() << '\n';
```

Supported selectors: root `$`, member (`.name` / `['name']`), array index incl.
negative, wildcard (`.*` / `[*]`), recursive descent (`..`), slices
(`[start:end:step]`), and unions. A **filter** expression (`[?...]`) is not yet
supported and throws. A syntactically malformed query throws `qbuem::parse_error`;
a valid query that matches nothing returns an empty vector. Returned Values view
into the same document as `root`, so keep the document alive.

---

## 🎬 SAX-style Event Visitor

`qbuem::visit(value, handler)` walks a parsed document depth-first and emits an
event for each node — for **transcoding, inspection, or folds** without
navigating the DOM by hand. Derive a handler from `qbuem::sax_handler` and
override only the events you want (the rest default to no-ops); dispatch is static
(no virtuals → zero overhead). Return `false` from any event to **abort** the walk.

```cpp
struct PriceSum : qbuem::sax_handler {
    double total = 0;
    bool key(std::string_view k) { want = (k == "price"); return true; }
    bool real(double v)    { if (want) total += v; want = false; return true; }
    bool integer(int64_t v){ if (want) total += v; want = false; return true; }
    bool want = false;
};

PriceSum h;
qbuem::sax_parse(json, h);     // parse + visit in one call
// or: qbuem::visit(alreadyParsedValue, h);
std::cout << h.total << '\n';
```

This is an event walk over the proven tape parser — **not** a second streaming
parser. For data larger than RAM, use [NDJSON](/guide/serialization#ndjson-json-lines)
(`read_lines`). String/key values are the raw on-the-wire slice (zero-copy),
matching `as<std::string_view>()`.

---

## ⚠️ Common Pitfalls

### 1. Doc Must Outlive Values

```cpp
// ❌ WRONG — doc is destroyed before root is used
qbuem::Value bad_parse() {
    qbuem::Document doc;
    return qbuem::parse(doc, R"({"x": 1})");
} // doc destroyed here → dangling pointer!

// ✅ CORRECT — keep them together
struct ParseResult {
    qbuem::Document doc;
    qbuem::Value root;
};
ParseResult good_parse(std::string_view json) {
    ParseResult r;
    r.root = qbuem::parse(r.doc, json);
    return r;
}
```

### 2. Array Indices — Plain `int` Works

All array index APIs (`operator[]`, `get()`, `erase()`) accept plain `int`, `unsigned int`, and `size_t`. No cast is needed:

```cpp
auto elem  = root["array"][0];   // ✅ plain int literal — works fine
auto elem2 = root["array"][1];   // ✅ same

int i = 2;
auto elem3 = root["array"][i];   // ✅ int variable — also works, no cast needed
```

Negative indices simply return an invalid (absent) value rather than throwing or invoking undefined behaviour:

```cpp
auto bad = root["array"][-1];    // ✅ safe — returns invalid Value{}
bad.is_valid();                   // false
```

### 3. SafeValue Size — Use `sv.size()`, Not `sv->size()`

`SafeValue` (returned by `.get()`) has its own `.size()` and `.empty()` that are **always safe** — they return `0` / `true` when the value is absent, without throwing.

```cpp
auto tags = root.get("tags");   // SafeValue — may be absent

// ❌ THROWS bad_optional_access when "tags" is missing
size_t n = tags->size();

// ✅ Returns 0 safely when absent
size_t n = tags.size();
bool   b = tags.empty();
```

This is the primary motivation for calling `.get()` instead of `operator[]` when you intend to chain or inspect size:

```cpp
// Full safe pattern — no branching needed
size_t tag_count = root.get("tags").size();     // 0 if key missing or not array
bool   has_tags  = !root.get("tags").empty();   // false if key missing

// Iterate only if present and non-empty
auto sv = root.get("results");
if (!sv.empty()) {
    for (auto elem : sv.value().elements()) {
        // process elem
    }
}
```

**Rule of thumb:** on a `SafeValue`, always call `.size()` and `.empty()` directly (without `->`).
