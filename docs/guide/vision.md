# Vision & Roadmap

qbuem-json aims to be the **fastest and safest single-header C++20 JSON library** — dual-engine
(flexible DOM + zero-tape struct mapping), with a **cross-language binary** story (CBOR) that few
competitors match. The roadmap below is curated against three rules: every feature must be
**general-purpose**, **differentiated**, and **free of over-engineering** (ship the smallest correct
version; single-header and the performance/safety bar are non-negotiable).

The full, sourced roadmap lives in [`ROADMAP.md`](https://github.com/qbuem/qbuem-json/blob/main/ROADMAP.md).

## Shipped ✅

- **Dual engine** — flat-tape SIMD DOM + **Nexus Fusion** (zero-tape direct JSON↔struct mapping).
- **SIMD** — AVX-512 · ARM NEON · **SVE** · SWAR structural indexing, with the Russ Cox (2026)
  unrounded-scaling float parser and Schubfach serialization.
- **CBOR (RFC 8949)** — binary codec from the same `QBUEM_JSON_FIELDS` registration, positional
  decode fast-path, generic/template types via `QBUEM_JSON_FIELDS_TPL`. Cross-language (e.g. JS `cbor-x`).
- **Standards** — JSON Pointer (RFC 6901), JSON Patch (RFC 6902), Merge Patch (RFC 7396),
  relaxed/comments parse mode, RFC 8259 well-formedness validation.
- **Integration** — `std::pmr` support; language bindings (Python/nanobind, Rust, Go); single-header,
  zero-dependency; continuously fuzzed + ASan/UBSan/TSan across x86-64 / aarch64 / Apple Silicon.

## Tier 1

- ✅ **Macro-free aggregate reflection** *(v1.14.0, GCC/Clang)* — plain aggregate structs serialize/deserialize across JSON and CBOR with no `QBUEM_JSON_FIELDS` (field names via reflection); the macro/registration always wins, so it is purely additive. Built for backend DTOs.
- ✅ **Rich error context** — `parse_error` exposes line/column + `format_error()` caret rendering *(v1.3.0)*.
- ✅ **serde-style field attributes** — enum (integer default + `QBUEM_JSON_ENUM` value names) *(v1.4.0)*, field rename `(member, "jsonKey")` + skip-by-omission *(v1.5.0)*, default-on-absent (built in).
- ✅ **NDJSON / JSON Lines** streaming — `read_lines` / `write_lines`, bounded memory *(v1.6.0)*.
- ✅ **Canonical JSON** — `qbuem::canonicalize`, deterministic bytes for hashing/signing (RFC 8785-style); CBOR is canonical by construction *(v1.7.0)*.

## Tier 2 — Mid-term

- ✅ **JSONPath (RFC 9535)** — structural selectors (root, member, index, wildcard, recursive descent, slice, union) *(v1.8.0)* + filter selectors `[?...]` (comparisons, existence tests, `&&`/`||`/`!`, `length()`/`count()`/`value()`) *(v1.13.0)*. The I-Regexp `match()`/`search()` functions are intentionally out of scope (no regex engine in a single-header lib).
- ✅ **WebAssembly build** — `bindings/wasm`: validate / minify / canonicalize / JSONPath in the browser & Node *(v1.9.0)*.
- ✅ **SAX-style event visitor** — `qbuem::visit` / `sax_parse` for transcoding & inspection *(v1.10.0)*.
- ❌ **MessagePack codec** — declined: duplicative of the existing CBOR codec (both binary-JSON); not worth a second parallel codec to maintain.
- ❌ **Runtime CPU dispatch** — declined: a header-only library is recompiled per project (`-march=native` → optimal SIMD for free); baking a multi-versioned dispatcher into the core hot path is high risk for a benefit only prebuilt artifacts see. See the [portable-vs-native build guide](/guide/bindings#portable-vs-native-builds-for-distributing-prebuilt-bindings) instead.

## Tier 3 — Longer-term

- ✅ **JSON diff / patch generation** — `qbuem::diff` + a complete functional RFC 6902 `apply_patch` (real-time state sync) *(v1.11.0)*.
- ✅ **Packaging + conformance** — `find_package(qbuem_json CONFIG)`, Conan recipe + vcpkg port *(v1.11.1)*, and a full [JSONTestSuite conformance run](/guide/correctness#jsontestsuite-conformance) (283/283 mandatory cases under ASan+UBSan).
- ❌ **JSON Schema** validation — **declined**: not differentiated (jsoncons / blaze / valijson already do it well), an L–XL subset would bloat the single header, and known C++ shapes get *compile-time* validation from `fuse<T>` already. We [document interop](/guide/correctness#json-schema-validation-interop) instead.
- ❌ **Schemaless On-Demand lazy cursor** — **declined**: a simdjson parity-chase; the flat-tape DOM + `fuse` already cover our cases, and the iterator-invalidation semantics are high regression risk.
- ⏸️ **Chunked / incremental (socket) parsing** — **deferred**: WebSocket frames and framed CBOR are already message-delimited, so a full message arrives before parse.
- ⏸️ **C++26 P2996 reflection backend** — **gated** to stable compilers (~2028): macro-free registration as an *opt-in fast-lane* (the macro stays the portable default).

The curated roadmap is **complete as of v1.13.0** — Tier 1, Tier 2, and the subset
of Tier 3 that fits the library's identity are shipped; everything else above is a
conscious decline/defer/gate. Further work is demand-driven, not roadmap-driven.

## Declined

Speculative or niche items we are intentionally **not** pursuing (would fail the guiding rules):
coroutine/async parsing, intra-document multithreading, constexpr JSON parsing; JMESPath / JSONata /
jq embedding (JSONPath — now with filters — covers the query need); BSON, Amazon Ion, Apache
Arrow/columnar, FlatBuffers/Cap'n Proto; and the old vision's "Protocol Shifting (SBE/FIX)", "IDL
Inference", and "Nexus Codegen". (SVE, language bindings, and `std::pmr` from the old vision are
**already shipped**.)
