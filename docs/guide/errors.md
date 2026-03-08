# Error Handling

Beast JSON provides two distinct philosophies for error handling, allowing developers to choose the best balance between performance and safety for their specific use case.

## 💥 The Strict Approach (Exceptions)

The `Value` API is designed for scenarios where you have a known schema or you are confident in the data's integrity. It prioritizes speed by assuming success but provides safety through standard C++ exceptions.

```cpp
try {
    auto doc = beast::json::parse(invalid_json);
    int id = doc["user"]["id"].as<int>();
} catch (const std::exception& e) {
    // Fails if:
    // 1. JSON is malformed
    // 2. Key "user" or "id" is missing
    // 3. "id" is not an integer
    std::cerr << "Error: " << e.what() << std::endl;
}
```

## 🛡️ The Monadic Approach (`SafeValue`)

The `SafeValue` API is designed for untrusted data and deep object traversal. It **never throws**. Instead, it propagates a "missing" state until you explicitly provide a fallback.

```cpp
// Created via .get() instead of operator[]
auto safe = doc.get("deeply").get("nested").get("value");

if (safe) {
    std::cout << "Value exists: " << safe.dump() << std::endl;
}

// Monadic fallback (zero overhead)
std::string result = safe.value_or("default_value");
```

### Why use one over the other?

| Feature | `Value` (Strict) | `SafeValue` (Monadic) |
| :--- | :--- | :--- |
| **Logic** | Throw on failure | Propagate failure |
| **Performance** | Maximum (minimal state) | High (propagating state) |
| **Syntax** | Concise `doc["key"]` | Monadic `.get("key")` |
| **Use Case** | Known Schemas / RPC | API Responses / Configs |

## 🧪 Validation & Checks

If you prefer a manual check approach, use the boolean type checkers:

```cpp
auto v = doc["id"];
if (v.is_int()) {
    process(v.as<int>());
} else if (v.is_valid()) {
    std::cerr << "Expected int, got type: " << v.type_name() << std::endl;
} else {
    std::cerr << "Key 'id' is missing" << std::endl;
}
```
