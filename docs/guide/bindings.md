# Language Bindings

Beast JSON is designed as a core engine. While written in C++20, it provides stable hooks for binding to other high-level languages without losing its performance advantage.

## 🐍 Python Bindings (via nanobind)

Python is the most common consumer of high-performance JSON engines. We recommend using **nanobind** for the leanest and fastest bindings.

### Example: Python C++ Extension
```cpp
#include <nanobind/nanobind.h>
#include "beast_json.hpp"

namespace nb = nanobind;

nb::object parse_to_python(std::string_view json) {
    auto doc = beast::json::parse(json);
    // Convert TapeDOM to Python Dict/List recursively...
}

NB_MODULE(beast_json_py, m) {
    m.def("parse", &parse_to_python);
}
```

## 🐹 Go Bindings (cgo)

For Go, you can wrap the single header with a small C-style shim to avoid C++ name mangling issues.

### Pattern: Fast-String Handoff
Since Go strings are immutable and contiguous, you can pass them to Beast JSON with zero overhead:
```go
import "C"
import "unsafe"

func Parse(jsonStr string) {
    ptr := C.CString(jsonStr)
    defer C.free(unsafe.Pointer(ptr))
    // Call C++ shim that uses beast::json::parse(std::string_view(ptr, len))
}
```

## 🦀 Rust Interop (via cxx)

Rust's **cxx** crate is the gold standard for C++ interop. Use it to share `beast::json::Value` views directly between Rust and C++.

```rust
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("beast_json.hpp");
        type Value;
        fn parse(json: &str) -> UniquePtr<Value>;
    }
}
```

## 🗺️ Future Binding Roadmap
We are planning official, performance-optimized wrappers for:
1. **Python**: Native wheels on PyPI.
2. **Node.js**: N-API based ultra-fast parser.
3. **Ruby**: Native extension for high-load Rails apps.
