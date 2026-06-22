# qbuem-json — API Reference {#mainpage}

This is the auto-generated **API reference** for `qbuem-json`, a single-header,
zero-dependency C++20 JSON + CBOR library. It is generated directly from the
library header, so it always matches the source.

- **Full guide, tutorials, and architecture:** <https://qbuem.com/qbuem-json/>
- **Source, releases, and issues:** <https://github.com/qbuem/qbuem-json>

## Where to start

Register a struct once with `QBUEM_JSON_FIELDS(T, field1, field2, ...)` (at
**namespace scope**, outside the struct) and it works across every entry point below:

| Task | API |
|------|-----|
| Parse to a navigable/mutable DOM | `qbuem::parse(Document&, std::string_view)` → `qbuem::Value` |
| Map JSON straight into a struct | `qbuem::read<T>()` (tape) · `qbuem::fuse<T>()` (zero-tape) |
| Reject missing required fields | `qbuem::read_strict<T>()` |
| Serialize a struct to JSON | `qbuem::write()` · `qbuem::write_to()` (reused buffer) |
| Binary codec (RFC 8949) | `qbuem::cbor::encode<T>()` / `qbuem::cbor::decode<T>()` |
| Canonical JSON (RFC 8785) | `qbuem::canonicalize()` |
| JSONPath query (RFC 9535) | `qbuem::query()` |
| JSON diff / patch (RFC 6902) | `qbuem::diff()` / `qbuem::apply_patch()` |

> **Lifetime note.** A `Value`/`Document` holds a non-owning view into the bytes you
> parse, so the input buffer must outlive every `Value`. Passing a temporary
> `std::string` to `parse` is a compile error (the rvalue overload is deleted); the
> copying `read<T>()` / `fuse<T>()` take a `string_view` and are safe with temporaries.

Browse the **Classes** and **Files** in the navigation for the complete reference.
Supported platforms: Linux (x86_64 / aarch64) and macOS (Apple Silicon), GCC and Clang.
