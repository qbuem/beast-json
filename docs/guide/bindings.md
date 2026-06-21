# Language Bindings

`qbuem-json` is built on a high-performance C++20 core, but it is designed to be accessible from any language. We provide native wrappers for major ecosystems and a **Stable C API** as the universal foundation.

---

## 🚀 Official Support Matrix

| Language | Binding Type | Maturity | Primary Use Case |
| :--- | :--- | :--- | :--- |
| **C++ &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;** | Native (Header-only) | Production | Backend services, low-latency systems, DTO (de)serialization |
| **C** | Shared Library (ABI) | Production | FFI Foundation, C Applications |
| **Python** | `nanobind` / `ctypes` | Beta | Data Science, Rapid Prototyping |
| **Rust** | `cxx` Bridge | Beta | Safe systems programming |
| **Go** | `cgo` Shim | Alpha | High-speed cloud services |
| **WebAssembly** | Emscripten / Embind | Beta | Browser & Node (validate, canonicalize, JSONPath) |

---

## 🕸️ WebAssembly (browser & Node)

`bindings/wasm/` compiles the engine to WebAssembly, exposing the operations
JavaScript lacks natively — fast RFC 8259 validation, compact/pretty
re-serialization, **RFC 8785-style canonicalization** (deterministic bytes for
signing/hashing), and **RFC 9535 JSONPath** queries — all over plain JSON strings.

```bash
cd bindings/wasm
./build.sh            # needs the Emscripten SDK → dist/qbuem_json.mjs + .wasm
node test/smoke.mjs
```

```js
import createQbuemJson from '@qbuem/json-wasm';
const Q = await createQbuemJson();

Q.validate('{"a":1}');                  // true
Q.canonicalize('{"b":1,"a":1.50}');     // '{"a":1.5,"b":1}'
Q.query('{"n":[10,20,30]}', '$.n[-1]'); // '[30]'   (JSON array string)
```

`minify` / `prettify` / `canonicalize` / `query` throw on invalid input;
`validate` returns a boolean. Since the browser already has `JSON.parse`, the
value here is **canonicalization** and **JSONPath** (plus a fast SIMD validator)
without bundling several JS libraries.

---

## 📦 Portable vs. native builds (for distributing prebuilt bindings)

qbuem-json picks its SIMD path **at compile time**, so a binary is tuned for the
ISA it was built against. When you *recompile in your own project* (the normal
header-only path), `-march=native` already gives the optimal kernel for that
machine — nothing else to do.

When you **distribute a prebuilt artifact** (a Python wheel, a Rust/Go shared
lib), choose deliberately:

| Goal | Build flags | Trade-off |
|:---|:---|:---|
| **Maximum speed**, known target | `-march=native` (or `-mavx2` / `-march=skylake-avx512`) | Fastest; **crashes** (illegal instruction) on a CPU lacking that ISA. |
| **Maximum compatibility**, any x86-64 | `-mssse3` / baseline `x86-64` (SWAR fallback) | Runs everywhere; gives up wide-SIMD throughput. |
| **Both** | Ship one artifact per ISA tier and select at install/load time | Most work; what manylinux wheels and distro packages do. |

There is intentionally **no runtime CPU dispatch baked into the header** — for a
header-only library the recompile path already yields optimal SIMD, and baking a
multi-versioned dispatcher into the core hot path would add risk for a benefit
only prebuilt artifacts see. Pick the build that matches how you ship.

---

## 🛠 Usage Guides

### 1. Stable C API (`bindings/c/`)
The C API is the "universal donor" used by most other bindings. It uses opaque handles for memory safety and a stable ABI.

```c
#include "qbuem_json_c.h"

// 1. Create a document (memory context)
QbuemJSONDocument* doc = qbuem_json_doc_create();

// 2. Parse JSON string
const char* json = "{\"version\": 1.0, \"tags\": [\"simd\", \"fast\"]}";
QbuemJSONValue root = qbuem_json_parse(doc, json, strlen(json), 0);

// 3. Navigate
QbuemJSONValue tags = qbuem_json_get_key(root, "tags");
QbuemJSONValue first = qbuem_json_get_idx(tags, 0);

printf("First tag: %s\n", qbuem_json_as_string(first, NULL));

// 4. Cleanup
qbuem_json_doc_destroy(doc);
```

### 2. Python Bindings (`bindings/python/`)
Python developers can choose between **convenience** (`ctypes`) and **raw speed** (`nanobind`).

#### High-Performance Extension (`nanobind`)
Direct C++ to Python mapping. Requires compiling the extension.
```bash
cmake -B build -DQBUEM_JSON_BUILD_PYTHON=ON
cmake --build build --target qbuem_json_py
```

#### Standard Wrapper (`ctypes`)
Requires only the C shared library. Zero build steps for the Python side.
```python
from qbuem_json import Document

doc = Document('{"status": "ok"}')
print(doc["status"]) # Output: ok
```

### 3. Rust Bindings (`bindings/rust/`)
Provides a safe, idiomatic Rust interface using the `cxx` bridge.

```rust
use qbuem_json::Document;

let mut doc = Document::new();
doc.parse(r#"{"data": 42}"#).unwrap();
let val = doc.root().get("data").as_i64();
```

---

## 💎 Object Serialization & Deserialization

`qbuem-json` provides high-level convenience functions that map JSON directly to your language's native types (structs, dicts, maps).

### 🐍 Python (Standard API)
The Python bindings offer a familiar `json`-like interface.

```python
from qbuem_json import loads, dumps

# Deserialization (JSON -> dict)
data = loads('{"name": "Alice", "active": true}')
print(data["name"])

# Serialization (dict -> JSON)
json_str = dumps({"version": 2.0, "code": "QJ"})
```

### 🌏 Go (Standard API)
The Go bindings support `Unmarshal` into structs and maps using reflection.

```go
import "github.com/qbuem/qbuem-json/bindings/go"

type Config struct {
    Port int    `json:"port"`
    Host string `json:"host"`
}

var cfg Config
qbuem.Unmarshal(jsonData, &cfg)

// Value-level serialization
root := doc.Root()
fmt.Println(root.Dump(2)) // Pretty print with 2-space indent
```

---

## 🏎 Performance: The "Lazy Advantage"

Traditional libraries like `encoding/json` or `json.loads` MUST parse the entire document into memory before you can access a single field. `qbuem-json` uses **Lazy Parsing**.

### Benchmark: Extracting 1 Field from 1MB JSON
| Method | Execution Time | Memory Overhead |
| :--- | :--- | :--- |
| **Python `json.loads`** | 12.4 ms | Large (Full AST) |
| **Go `encoding/json`** | 8.2 ms | Large (Full Struct) |
| **qbuem-json (Lazy)** | **0.15 ms** | **Zero** (Pointer View) |

> [!TIP]
> **When to use what?**
> - Use **`loads()` / `Unmarshal()`** when you need *every* field in a small document.
> - Use **`Value` access (`root["key"]`)** when you need *specific* fields or are dealing with large datasets. It is up to **80x faster** for random access.

---

## 🧪 The "Core Binding Guide" (Technical Deep Dive)

If your language isn't listed above, you can build a binding in minutes using the **Stable C API**. Follow these principles:

### A. Mapping Opaque Handles
The C API uses two primary types:
- `QbuemJSONDocument*`: A memory arena. Always map this as a pointer (IntPtr, void*, etc.).
- `QbuemJSONValue`: A small POD struct (64-bit). **Do not** allocate this on the heap. Pass it by value.

```c
typedef struct {
    void* state;
    uint32_t index;
} QbuemJSONValue;
```

### B. Memory Ownership
1. **The Document is the Owner**: All `QbuemJSONValue` handles returned by the API point to memory held by the `QbuemJSONDocument`.
2. **Safety Rule**: If you destroy the `Document`, all associated `Value` objects become invalid (UAF).
3. **Strings**: Functions returning `const char*` return pointers into the internal Tapestry. They are valid as long as the Document exists and is not modified.

### C. Creating a New Binding (Step-by-Step)
1. **Load Lib**: Use your language's FFI (e.g., `DllImport` in C#, `Foreign.Lib` in Haskell).
2. **Define Value Struct**: Standardize the 12-byte (or 16-byte padded) `QbuemJSONValue` structure.
3. **Wrap the Doc**: Create a class with a finalizer/destructor that calls `qbuem_json_doc_destroy`.
4. **Proxy Methods**: Map `qbuem_json_get_key` to your language's `operator[]` or `get()` method.

> [!IMPORTANT]
> Always use `size_t*` version of `qbuem_json_as_string` for binary safety. `qbuem-json` supports internal null bytes in strings.

---

