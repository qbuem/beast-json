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

- ✅ **Rich error context** — `parse_error` exposes line/column + `format_error()` caret rendering *(v1.3.0)*.
- ✅ **serde-style field attributes** — enum (integer default + `QBUEM_JSON_ENUM` value names) *(v1.4.0)*, field rename `(member, "jsonKey")` + skip-by-omission *(v1.5.0)*, default-on-absent (built in).
- ✅ **NDJSON / JSON Lines** streaming — `read_lines` / `write_lines`, bounded memory *(v1.6.0)*.
- ✅ **Canonical JSON** — `qbuem::canonicalize`, deterministic bytes for hashing/signing (RFC 8785-style); CBOR is canonical by construction *(v1.7.0)*.

## Tier 2 — Mid-term

- 🚧 **JSONPath (RFC 9535)** — structural selectors (root, member, index, wildcard, recursive descent, slice, union) shipped *(v1.8.0)*; filter expressions planned.
- ✅ **WebAssembly build** — `bindings/wasm`: validate / minify / canonicalize / JSONPath in the browser & Node *(v1.9.0)*.
- ✅ **SAX-style event visitor** — `qbuem::visit` / `sax_parse` for transcoding & inspection *(v1.10.0)*.
- ❌ **MessagePack codec** — declined: duplicative of the existing CBOR codec (both binary-JSON); not worth a second parallel codec to maintain.
- ❌ **Runtime CPU dispatch** — declined: a header-only library is recompiled per project (`-march=native` → optimal SIMD for free); baking a multi-versioned dispatcher into the core hot path is high risk for a benefit only prebuilt artifacts see. See the [portable-vs-native build guide](/guide/bindings#portable-vs-native-builds-for-distributing-prebuilt-bindings) instead.

## Tier 3 — Longer-term

- ✅ **JSON diff / patch generation** — `qbuem::diff` + a complete functional RFC 6902 `apply_patch` (real-time state sync) *(v1.11.0)*.
- ⬜ **JSON Schema** (Draft-7 → 2020-12 subset) validation — OpenAPI / MCP alignment.
- ⬜ **Schemaless On-Demand lazy cursor** — parse only the values you touch.
- ⬜ **Chunked / incremental (socket) parsing** — resumable feeding from a partial buffer.
- ⬜ **Packaging** — vcpkg + Conan registry; explicit JSONTestSuite conformance badge.
- ⬜ **C++26 P2996 reflection backend** — macro-free registration as an *opt-in fast-lane* once stable
  compilers ship (the macro stays the portable default).

## Declined

Speculative or niche items we are intentionally **not** pursuing (would fail the guiding rules):
coroutine/async parsing, intra-document multithreading, constexpr JSON parsing; JMESPath / JSONata /
jq embedding (JSONPath covers the query need); BSON, Amazon Ion, Apache Arrow/columnar,
FlatBuffers/Cap'n Proto; and the old vision's "Protocol Shifting (SBE/FIX)", "IDL Inference", and
"Nexus Codegen". (SVE, language bindings, and `std::pmr` from the old vision are **already shipped**.)
