# Type Mapping & Macros

qbuem-json uses a **Zero-Boilerplate Design**. It automatically deduces exactly how your C++ types should be represented in JSON — no configuration, no registration, no code generation step.

## 🪄 STL Type Mapping Schema

You don't need to write any conversion code for standard library containers. Just pass them to `qbuem::write()` or `qbuem::read<T>()`.

| C++ Type | JSON Schema | Example Input | JSON Output |
| :--- | :--- | :--- | :--- |
| **`std::vector<T>`**<br/>`std::list`, `std::deque` | Array `[ ... ]` | `{1, 2, 3}` | `[1,2,3]` |
| **`std::array<T, N>`** | Fixed Array `[ ... ]` | `{1.0, 2.0}` | `[1.0,2.0]` |
| **`std::set<T>`**<br/>`std::unordered_set` | Sorted Array | `{3, 1, 2}` | `[1,2,3]` |
| **`std::map<string, T>`**<br/>`std::unordered_map` | Object `{ ... }` | `{{"a", 1}}` | `{"a":1}` |
| **`std::optional<T>`** | Value or `null` | `std::nullopt` | `null` |
| **`std::tuple<T...>`** | Heterogeneous Array | `{1, "A", true}` | `[1,"A",true]` |
| **`std::pair<T1, T2>`** | 2-element Array | `{"key", 42}` | `["key",42]` |
| **`std::variant<T...>`** | Dynamic Value Match | `(holds int) 123` | `123` |

> [!TIP]
> **This mapping is fully recursive!** A `std::map<std::string, std::vector<std::optional<int>>>` works perfectly out of the box.
>
> ```cpp
> auto data = qbuem::read<std::map<std::string, std::vector<int>>>(
>     R"({"scores": [95, 87, 100], "ids": [1, 2]})"
> );
> // data["scores"][0] == 95
> ```

---

## 🛠️ Choice of Engines: DOM vs. Nexus

When mapping JSON to custom structs, qbuem-json allows you to choose your performance profile based on your data scale.

### 1. The Standard Path: `qbuem::read<T>` (Tape-DOM)
Best for **general-purpose bulk data**, large arrays, or when schemas are slightly fluid. It uses the Stage 1 SIMD scanner to build a contiguous Tape, which is then mapped to your struct.

```cpp
// 1. Build Tape (SIMD)
// 2. Map to Struct
User u = qbuem::read<User>(json_str);
```

### 2. The Nexus Path: `qbuem::fuse<T>` (Zero-Tape)
Best for **latency-critical micro-DTOs**. It bypasses the Tape entirely, using **Nexus Fusion** technology to stream JSON directly into your struct members using $O(1)$ perfect-hash dispatch.

```cpp
// 1. Direct Stream-to-Struct mapping
// 0.0 Tape Allocation
User u = qbuem::fuse<User>(json_str);
```

> [!NOTE]
> On invalid input `read<T>` / `fuse<T>` throw [`qbuem::parse_error`](/guide/errors#parse-errors)
> (a `std::runtime_error` with `offset()`). For [duplicate object keys](/guide/parsing#duplicate-keys),
> `fuse<T>` is **last-wins** (it overwrites the field) while the DOM `read<T>` is first-wins.

---

## ✨ Macro-Free Mapping (Aggregate Reflection)

*(v1.14.0+, GCC/Clang)* For a **plain aggregate** — a struct with public members, no
user-declared constructors, and no base classes — you don't need any macro at all.
`qbuem::write` / `read` / `read_strict`, the zero-tape `qbuem::fuse`, and
`cbor::encode` / `decode` reflect the fields (and recover their **names**, used as
JSON keys) automatically:

```cpp
struct CreateUserReq {            // no QBUEM_JSON_FIELDS needed
    std::string        name;
    int                age;
    std::optional<int> referrer;
};

std::string j = qbuem::write(CreateUserReq{"Ada", 30, std::nullopt});
// {"name":"Ada","age":30,"referrer":null}
CreateUserReq r = qbuem::read<CreateUserReq>(body);   // request DTO, zero boilerplate
std::string bin = qbuem::cbor::encode(r);             // same fields → CBOR
CreateUserReq f = qbuem::fuse<CreateUserReq>(body);   // zero-tape, still no macro
```

This is built for backends with many request/response DTOs: define the struct, serialize
it. Nested aggregates, **generic / template** aggregates (each instantiation reflects on
its own), `std::optional`, and STL containers all work — across every engine above.

| Detail | Behaviour |
|:---|:---|
| **Precedence** | A `QBUEM_JSON_FIELDS` / `from_qbuem_json` registration, if present, **always wins** — reflection is a fallback, so this is purely additive. |
| **When to use the macro instead** | Field **renaming** (`(member, "jsonKey")`), **skip**, non-aggregates (private members / constructors), or `> 32` fields still require `QBUEM_JSON_FIELDS`. |
| **`fuse<T>` performance** | `fuse<T>` works macro-free for aggregates too (generic and nested). Its key→field routing is a reflected hash-ladder rather than the macro's compile-time `switch`, so for **very wide hot-path structs** the macro (`QBUEM_JSON_FIELDS` / `QBUEM_JSON_FIELDS_TPL`) stays the peak-dispatch path (≈1.5× faster on a realistic DTO; the gap grows with field count). |
| **Requirements** | Aggregate, default-constructible, ≤ 32 fields. Works for any linkage (incl. anonymous-namespace / function-local types) and non-`constexpr` members (`std::map`, …). |
| **Why GCC/Clang only** | Field names are recovered from `__PRETTY_FUNCTION__`. The library is GCC/Clang-only by design, so this needs no MSVC fallback — the no-Windows scope *enables* the feature rather than limiting it. |

## ✅ Validating request DTOs

At an API boundary you parse untrusted JSON into a DTO and then must validate it.
qbuem-json splits this into two layers — it owns the part that can't be done after
the fact, and leaves business rules to a couple of explicit lines.

**Required fields — `read_strict<T>` *(v1.15.0+)*.** Plain `read<T>` is lenient: a
missing field is silently left default-constructed, so after deserializing you
can't tell whether `age == 0` means "they sent 0" or "they sent nothing". That
gap can't be closed in handler code. `read_strict<T>` closes it — a missing
**required** field throws `qbuem::parse_error`. A field is required unless it is a
`std::optional`; the check nests through aggregates and present optionals:

```cpp
struct Addr { std::string city; int zip; };
struct CreateUserReq {
    std::string             email;      // required
    int                     age;        // required
    std::optional<std::string> referrer; // optional — absence is fine
    Addr                    addr;       // required, and nested-checked
};

auto req = qbuem::read_strict<CreateUserReq>(body);  // throws if email/age/addr.* absent
```

`read_strict<T>` is reflection-driven, so it applies to plain aggregates (not
`QBUEM_JSON_FIELDS`-registered structs). It does not descend into list/map
elements.

**Value rules — keep them in the handler.** Range, length, format, and enum
checks are deliberately *not* a library feature: they belong next to the handler
where the precise error message lives, they're two lines, and a constraint
language tends to balloon (regex, cross-field rules, error aggregation, i18n). If
you want that, reach for a dedicated validator (reflect-cpp, a JSON Schema
library). The idiomatic pattern:

```cpp
auto req = qbuem::read_strict<CreateUserReq>(body);
if (req.age < 0 || req.age > 150)      return bad_request("age out of range");
if (req.email.find('@') == npos)       return bad_request("email malformed");
// ... use req
```

## 🏗️ Direct Struct Mapping with `QBUEM_JSON_FIELDS`

For your own types — or whenever you need renaming, skip, the `fuse<T>` fast path, or
more than 32 fields — the `QBUEM_JSON_FIELDS` macro auto-generates optimized metadata used by both engines. It **must** be placed **outside** the struct definition, at namespace scope.

```cpp
struct Address {
    std::string street;
    std::string city;
    std::string country;
};
QBUEM_JSON_FIELDS(Address, street, city, country)   // ← namespace scope, after closing }

struct User {
    uint64_t    id;
    std::string username;
    Address     address;                              // nested struct — works automatically!
    std::vector<std::string> tags;                   // STL container — works automatically!
    std::optional<double>    score;                  // optional — maps to null when empty
    bool        active = true;                       // default values are preserved
};
QBUEM_JSON_FIELDS(User, id, username, address, tags, score, active)
```

::: warning Inside vs outside the struct
`QBUEM_JSON_FIELDS` expands to **free function** definitions (`from_qbuem_json`, `to_qbuem_json`, etc.). Placing it **inside** a struct body turns those into member functions, which breaks Argument-Dependent Lookup (ADL) — `qbuem::read<T>()` and `qbuem::write()` rely on ADL to find the free functions and **will not compile** if they are members.

```cpp
// ❌ WRONG — inside struct body → member functions → ADL breaks
struct User {
    int id;
    std::string name;
    QBUEM_JSON_FIELDS(User, id, name)   // compile error: no matching read/write
};

// ✅ CORRECT — outside struct, at namespace scope
struct User {
    int id;
    std::string name;
};
QBUEM_JSON_FIELDS(User, id, name)       // free functions generated → ADL works
```
:::

> [!IMPORTANT]
> To use `qbuem::fuse<T>`, the struct **must** be registered with `QBUEM_JSON_FIELDS` and use C++20 standard layout types where possible for maximum speed.

### Performance Tip: Perfect Hash Dispatch
Unlike other libraries that use runtime string comparisons, `QBUEM_JSON_FIELDS` computes **FNV-1a hashes at compile-time**. Whether your struct has 3 fields or 30, field lookup is always $O(1)$.

> [!NOTE]
> `QBUEM_JSON_FIELDS` now supports up to **32 fields** per struct. For larger structures, see the manual ADL hooks section below.

### Generic (template) types: `QBUEM_JSON_FIELDS_TPL` {#generic-template-types}

`QBUEM_JSON_FIELDS` registers a single concrete type. To register a **class template** so that *every* instantiation works — without repeating the macro per type — use `QBUEM_JSON_FIELDS_TPL`. Pass the template-parameter list and the dependent type **each wrapped in parentheses** (so their commas survive as one macro argument), then the field names:

```cpp
template <typename T>
struct Box { T v; int tag; };
QBUEM_JSON_FIELDS_TPL((typename T), (Box<T>), v, tag)   // one line covers Box<int>, Box<std::string>, …

template <typename A, typename B>
struct Pair { A a; B b; };
QBUEM_JSON_FIELDS_TPL((typename A, typename B), (Pair<A, B>), a, b)
```

```cpp
auto bytes = qbuem::cbor::encode(Box<int>{42, 7});      // works…
auto a     = qbuem::read<Box<std::string>>(json);        // …for every instantiation
auto f     = qbuem::fuse<Box<double>>(json);             // including the zero-tape engine
```

It emits the same ADL surface as `QBUEM_JSON_FIELDS` (DOM `read`/`write`, Nexus `fuse`, FastWriter, and [CBOR](./cbor) `encode`/`decode`) but as **function templates**, so ADL resolves them for any instantiation. Nesting works too — `Envelope<Box<int>>`, `std::vector<Box<int>>`, etc.

> [!NOTE] Zero runtime overhead
> `QBUEM_JSON_FIELDS_TPL` is a **compile-time convenience only**. The compiler instantiates the templates to byte-for-byte the same code path as a concrete `QBUEM_JSON_FIELDS` registration. Measured on a structurally-identical 7-field type (concrete vs. template instantiation, same payload), encode/decode/`fuse` latency matched within noise (≤0.5 %) and the produced wire bytes were identical across both JSON and CBOR. See [Benchmarks → Generic types](./benchmarks#generic-types-zero-overhead).

::: warning
- Like `QBUEM_JSON_FIELDS`, invoke it at **namespace scope** in the same namespace as the template (ADL).
- Don't *also* register a concrete instantiation of the same template with `QBUEM_JSON_FIELDS` — the template and the concrete overload would be ambiguous.
- The parentheses around the two type arguments are required; they are what lets multi-parameter templates (`(Pair<A, B>)`) pass through the preprocessor. A bare `QBUEM_JSON_FIELDS(Pair<int, std::string>, …)` does **not** compile because the preprocessor splits on the comma inside `<…>` — wrap multi-param instantiations in a `using` alias, or use `QBUEM_JSON_FIELDS_TPL`.
:::

### Renaming & skipping fields {#rename-skip}

By default a field's JSON/CBOR key is its C++ member name. To use a **different
wire key**, write the field as `(member, "jsonKey")` — useful for snake_case C++
↔ camelCase JSON, reserved words, or matching a third-party API:

```cpp
struct User {
    int64_t     id;
    std::string user_name;   // C++ snake_case
    int         level;
};
QBUEM_JSON_FIELDS(User, (id, "userId"), (user_name, "userName"), level)

qbuem::write(User{7, "neo", 42});
// {"userId":7,"userName":"neo","level":42}
```

A bare field name behaves exactly as before, so renaming is opt-in per field and
mixes freely with bare fields. Rename works across **DOM, `fuse`, and CBOR**, and
composes with [`QBUEM_JSON_FIELDS_TPL`](#generic-template-types).

**Skipping a field needs no syntax** — simply omit it from the list. A member
that isn't registered is neither serialized nor read (and is left at its default
on read). So computed/internal members just stay off the macro:

```cpp
struct Account {
    std::string name;
    std::string password_hash;  // not listed → never serialized or read
};
QBUEM_JSON_FIELDS(Account, name)
```

### Enums {#enums}

Any `enum` / `enum class` works out of the box, serializing as its **underlying
integer** across every engine (DOM, `fuse`, CBOR):

```cpp
enum class Dir { North, East, South, West };
struct Mob { int id; Dir facing; };
QBUEM_JSON_FIELDS(Mob, id, facing)

qbuem::write(Mob{7, Dir::West});   // {"id":7,"facing":3}
```

To serialize an enum as its **value name** instead, register it once with
`QBUEM_JSON_ENUM` (at the enum's namespace scope, like `QBUEM_JSON_FIELDS`):

```cpp
enum class Color { Red, Green, Blue };
QBUEM_JSON_ENUM(Color, Red, Green, Blue)

qbuem::write(Color::Green);                 // "Green"
qbuem::read<Color>("\"Blue\"");             // Color::Blue
qbuem::cbor::encode(Color::Red);            // CBOR text "Red"
```

It round-trips by name across JSON, `fuse`, and CBOR. On read, an unknown name
leaves the field at its default (DOM) or throws `qbuem::parse_error` (CBOR /
strict `fuse`). The names are the C++ enumerator identifiers; for custom wire
strings, define the two ADL hooks yourself (`qbuem_enum_to_string(E)` and
`qbuem_enum_from_string(std::string_view, E&)`).

> [!NOTE] Default values are automatic
> When a registered field's key is **absent** from the input object, qbuem-json
> leaves that field at its default — no annotation needed. So a missing `"facing"`
> keeps whatever the struct's default-initialized `Dir` is.

---

## ✏️ Mutating Parsed JSON

qbuem-json supports **non-destructive mutations** — the original tape is immutable, and changes are stored in a fast overlay map.

### Scalar Mutation

```cpp
qbuem::Document doc;
auto root = qbuem::parse(doc, R"({"user": {"id": 1, "name": "Alice", "score": 87.5}})");

// Override scalar values
root["user"]["id"]    = 99;
root["user"]["name"]  = "Bob";
root["user"]["score"] = 100.0;
root["user"]["active"] = true;    // can add new scalar fields
root["user"]["extra"] = nullptr;  // null

// Immediately reflected in dump()
std::cout << root["user"].dump() << "\n";
// {"id":99,"name":"Bob","score":100.0,"active":true,"extra":null}

// Restore original parsed value
root["user"]["id"].unset();
std::cout << root["user"]["id"].as<int>() << "\n"; // 1 (original restored)
// ⚠️  unset() reverts to the *original parsed value*, NOT null.
//     After unset(), type_name() and as<T>() reflect the original tape entry.
//     unset() only removes the scalar mutation overlay; keys added via insert()
//     or elements added via push_back() are NOT affected by unset().
```

### Structural Mutations (Add / Remove / Append)

```cpp
qbuem::Document doc;
auto root = qbuem::parse(doc, R"({"tags": ["cpp", "json"], "config": {}})");

// Object: add new key
root.insert("version", 2);
root.insert("label", std::string_view{"preview"});

// Object: add a nested JSON subtree
root.insert_json("meta", R"({"build": "release", "arch": "x86_64"})");

// Object: remove a key
root.erase("deprecated_field");

// Array: append elements
root["tags"].push_back(std::string_view{"simd"});              // "simd"
root["tags"].push_back_json(R"({"nested": "object"})");        // object element

// Array: remove by index (accepts size_t or unsigned int)
root["tags"].erase(0u);  // removes "cpp"

// All changes reflected immediately
std::cout << root.dump() << "\n";
// size() reflects both tape elements AND push_back() additions
std::cout << root["tags"].size() << "\n"; // 3 (original 2 + 1 push_back)
// items() includes both tape keys AND insert() additions
for (auto [k, v] : root.items()) { /* iterates original + inserted keys */ }
```

### Merging JSON (RFC 7396 Merge Patch)

```cpp
qbuem::Document doc;
auto root = qbuem::parse(doc, R"({"a": 1, "b": 2, "c": 3})");

// merge_patch: adds/updates fields from patch, removes fields set to null
root.merge_patch(R"({"b": 99, "c": null, "d": "new"})");

std::cout << root.dump() << "\n";
// {"a":1,"b":99,"d":"new"}  ("c" was removed because it was null in the patch)
```

### Diff & patch (RFC 6902) — `qbuem::diff` / `qbuem::apply_patch`

`qbuem::diff(from, to)` computes the JSON Patch (RFC 6902) that turns one document
into another — send **only the delta** for real-time state sync. `qbuem::apply_patch`
applies a patch and returns the new document:

```cpp
std::string before = R"({"hp":100,"pos":[0,0],"buffs":["haste"]})";
std::string after  = R"({"hp":80,"pos":[3,0],"buffs":["haste","shield"]})";

std::string patch = qbuem::diff(before, after);
// [{"op":"replace","path":"/hp","value":80},
//  {"op":"replace","path":"/pos/0","value":3},
//  {"op":"add","path":"/buffs/-","value":"shield"}]

std::string result = qbuem::apply_patch(before, patch);   // == after
```

`apply_patch` is a **complete, functional** RFC 6902 implementation (`add`, `remove`,
`replace`, `move`, `copy`, `test`): it rebuilds the document per op, so multi-op
patches, array-index removal, JSON-Pointer `~0`/`~1` unescaping, and
whole-document replacement all behave correctly. A failing op (bad path, failed
`test`) throws — a patch is all-or-nothing. `apply_patch(A, diff(A, B))` always
reproduces `B`.

> Numbers compare by value (`1` == `1.0`) and strings by decoded text, so `diff`
> never emits spurious ops for representation-only differences. The cross-language
> story holds too: the generated patch is standard RFC 6902, so a JS/TS client can
> apply it with any conformant library.

---

## 🔌 Advanced: ADL & Custom Hooks (Nexus Engine)

If you need to support third-party types that cannot be modified with macros, or if your struct exceeds 32 fields, you can manually implement the **ADL Hooks**.

The Nexus Engine (`qbuem::fuse`) specifically looks for `nexus_pulse` via Argument-Dependent Lookup.

```cpp
// Manual Nexus Hook for a 3rd party type
namespace third_party {
    struct Custom { int x; };

    inline void nexus_pulse(std::string_view key, const char*& p, const char* end, Custom& obj) {
        // Same key hash the macro uses: fast_key_hash at runtime,
        // fast_key_hash_ce (consteval) for the case labels. After a hash
        // match, verify the raw bytes — exactly as the generated dispatch
        // does — so a hash collision can never mis-route a field.
        switch (qbuem::json::detail::fast_key_hash(key)) {
            case qbuem::json::detail::fast_key_hash_ce("x"):
                if (key == "x")
                    qbuem::json::detail::from_json_direct(p, end, obj.x);
                else
                    qbuem::json::detail::skip_direct(p, end);
                break;
            default:
                qbuem::json::detail::skip_direct(p, end);
                break;
        }
    }
}
```

By defining `nexus_pulse` in the same namespace as your type, `qbuem::fuse<T>` will automatically use it for direct, zero-tape mapping. 

---

## 🔧 Third-Party Types via ADL (DOM Engine)

If you **cannot** modify a struct (e.g., from a library like `glm`), define Argument-Dependent Lookup (ADL) functions in the **same namespace** as the type:

```cpp
#include <glm/vec3.hpp>

namespace glm {

    // Teach qbuem-json how to parse glm::vec3 from [x, y, z]
    inline void from_qbuem_json(const qbuem::Value& v, vec3& out) {
        out.x = v[0u].as<float>();
        out.y = v[1u].as<float>();
        out.z = v[2u].as<float>();
    }

    // Teach qbuem-json how to serialize glm::vec3 to [x, y, z]
    inline void to_qbuem_json(qbuem::Value& root, const vec3& in) {
        root = qbuem::Value::array();
        root.push_back(qbuem::Value(in.x));
        root.push_back(qbuem::Value(in.y));
        root.push_back(qbuem::Value(in.z));
    }
}

// Now glm::vec3 works with qbuem-json natively!
glm::vec3 pos = qbuem::read<glm::vec3>("[1.0, 2.0, 3.5]");
std::string json = qbuem::write(pos);  // "[1.0,2.0,3.5]"
```

This also works for nested structs containing third-party types:

```cpp
struct Transform {
    glm::vec3 position;
    glm::vec3 rotation;
};
QBUEM_JSON_FIELDS(Transform, position, rotation)  // glm::vec3 is automatically handled!
```

---

## 🧩 Complete Real-World Example

Here's how qbuem-json handles a full backend account-API response:

```cpp
struct Usage {
    int64_t api_calls  = 0;
    int64_t storage_mb = 0;
    double  cost_usd   = 0.0;
};
QBUEM_JSON_FIELDS(Usage, api_calls, storage_mb, cost_usd)

struct Plan {
    std::string tier;
    std::string renews_on;
    std::vector<std::string> features;
};
QBUEM_JSON_FIELDS(Plan, tier, renews_on, features)

struct Account {
    uint64_t    id;
    std::string email;
    int         seats   = 1;
    Usage       usage;
    Plan        plan;
    std::optional<std::string> org;
    std::vector<std::string>   roles;
};
QBUEM_JSON_FIELDS(Account, id, email, seats, usage, plan, org, roles)

int main() {
    // --- Deserialize an API response ---
    std::string api_json = R"({
        "id": 123456, "email": "ada@example.com", "seats": 25,
        "usage": {"api_calls": 1840221, "storage_mb": 4096, "cost_usd": 119.90},
        "plan": {"tier": "business", "renews_on": "2026-07-01", "features": ["sso", "audit-log"]},
        "org": "Acme Inc",
        "roles": ["admin", "billing"]
    })";

    Account a = qbuem::read<Account>(api_json);
    std::cout << a.email << " (" << a.seats << " seats)\n";
    std::cout << "Spend: $" << a.usage.cost_usd << "\n";
    std::cout << "Org: " << a.org.value_or("(personal)") << "\n";

    // --- Modify and serialize back ---
    a.seats = 30;
    a.roles.push_back("auditor");

    std::string updated_json = qbuem::write(a, 2); // pretty-print
    std::cout << updated_json << "\n";
}
```
