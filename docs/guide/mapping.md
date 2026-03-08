# Object Mapping (Macros)

Beast JSON provides a powerful, zero-boilerplate way to map C++ structs to JSON using the `BEAST_JSON_FIELDS` macro. This leverages C++20 standard compile-time reflection patterns.

## 🛠️ The `BEAST_JSON_FIELDS` Macro

Simply place the macro inside your struct or class. It automatically generates optimized `from_beast_json` and `to_beast_json` functions.

```cpp
struct UserProfile {
    uint64_t id;
    std::string username;
    std::vector<std::string> tags;
    bool active;

    // Registers all fields for automation
    BEAST_JSON_FIELDS(id, username, tags, active)
};
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
