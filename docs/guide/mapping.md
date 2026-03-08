# Type Mapping & Macros

Beast JSON uses a **Zero-Boilerplate Design**. It automatically deduces exactly how your C++ types should be represented in JSON.

## 🪄 STL Type Mapping Schema

You don't need to write conversion code for standard library containers. Just pass them to `dump()` or `parse()`!

| C++ Type | JSON Schema | C++ Example | JSON Example |
| :--- | :--- | :--- | :--- |
| **`std::vector<T>`**<br/>`std::list`, `std::array` | **Array** `[ ... ]` | `std::vector<int> v = {1, 2};` | `[1, 2]` |
| **`std::map<string, T>`**<br/>`std::unordered_map` | **Object** `{ ... }` | `std::map<std::string, int> m = {{"a", 1}};` | `{"a": 1}` |
| **`std::optional<T>`** | **Value / null** | `std::optional<int> o = std::nullopt;` | `null` |
| **`std::tuple<T1, T2>`**<br/>`std::pair<T1, T2>` | **Heterogeneous Array** | `std::tuple<int, string> t = {1, "A"};` | `[1, "A"]` |
| **`std::variant<T...>`** | **Dynamic Value Match** | `std::variant<int, string> v = 123;` | `123` |

> [!TIP]
> **This mapping is recursive!** A complex type like `std::map<std::string, std::vector<std::optional<int>>>` works perfectly out of the box with zero configuration.

---

## 🛠️ Custom Structs (`BEAST_JSON_FIELDS`)

For your own domain objects, Beast JSON provides a powerful macro that leverages C++20 reflection patterns to automatically generate optimized mapping code.
## 🛠️ The `BEAST_JSON_FIELDS` Macro

Simply place the macro inside your struct or class. It automatically generates optimized `from_beast_json` and `to_beast_json` functions.

```cpp
struct UserProfile {
    uint64_t id;
    std::string username;
    std::vector<std::string> tags;
    bool active;
};

// Registers all fields for automation
BEAST_JSON_FIELDS(UserProfile, id, username, tags, active)
```

### 🏎️ Why use the macro?
1. **Performance**: The macro's generated code is 100% inlined and uses direct tape-to-memory paths.
2. **Type Safety**: Automatic type checking for every field during parsing.
3. **Implicit Nesting**: If a struct contains another struct which also has `BEAST_JSON_FIELDS`, the mapping works recursively.

## 📦 Usage Examples

### Serialization (Struct to JSON)
```cpp
UserProfile profile{123, "beast_master", {"cpp20", "simd"}, true};
std::string json = beast::json::dump(profile);
```

### Deserialization (JSON to Struct)
```cpp
std::string input = R"({"id": 123, "username": "beast_master", "tags": ["cpp20"], "active": true})";
auto doc = beast::json::parse(input);

UserProfile profile;
doc.as(profile); // Maps JSON to your struct
```

## 🛠️ Advanced: Manual ADL Mapping

If you cannot modify a struct (e.g., from a third-party library), you can implement **ADL (Argument-Dependent Lookup)** functions in the same namespace as the type.

```cpp
namespace third_party {
    struct ExternalType { int x; };

    void to_beast_json(beast::json::Value& v, const ExternalType& t) {
        v.set(t.x);
    }

    void from_beast_json(const beast::json::Value& v, ExternalType& t) {
        t.x = v.as<int>();
    }
}
```
Beast JSON will automatically pick these up during `dump()` and `as()`.
