# Parsing & Access

Beast JSON provides two primary ways to interact with JSON data: the **Strict API** (`Value`) and the **Safe API** (`SafeValue`).

## 🔍 Parsing Entry Points

### `beast::json::parse`
The most common way to parse JSON. It allocates a new `Document` internally.

```cpp
auto doc = beast::json::parse(R"({"status": "ok"})");
```

### `beast::json::parse_reuse`
Optimized for high-frequency loops. Reuses existing document memory.

```cpp
beast::json::Document doc;
beast::json::parse_reuse(doc, json_string);
```

## 🎯 Navigating the DOM

### Strict Access (`Value`)
Returns a `Value` handle. If the key is missing, it returns an invalid handle. Calling `.as<T>()` on an invalid handle or a type mismatch will throw `std::runtime_error`.

```cpp
auto val = doc["user"]["id"];
if (val.is_int()) {
    int id = val.as<int>();
}
```

### Safe Monadic Access (`SafeValue`)
Perfect for deep traversal where you don't want to check every step. It never throws.

```cpp
// Returns "guest" if any step in the chain is missing or not a string
std::string role = doc.get("metadata").get("permissions").get("role").value_or("guest");
```

## 🏗️ Type Checkers

Every `Value` provides a suite of non-throwing type checkers:
- `is_object()` / `is_array()`
- `is_string()` / `is_number()`
- `is_int()` / `is_double()`
- `is_bool()` / `is_null()`
- `is_valid()` (Returns false if the node doesn't exist)
