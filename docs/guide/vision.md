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

## Tier 1 — Next (v1.3)

- ⬜ **Rich error context** — line/column + JSON-Pointer path + a caret snippet (today: byte offset).
- ⬜ **serde-style field attributes** — enum⇄string, rename/alias, skip, default (C++20, zero hot-path cost).
- ⬜ **NDJSON / JSON Lines** streaming — multi-GB in bounded memory (logs, ML, LLM output).
- ⬜ **Deterministic / canonical CBOR** + **JSON Canonicalization (RFC 8785 / JCS)** — reproducible bytes for signing & hashing.

## Tier 2 — Mid-term (v1.4–1.5)

- ⬜ **JSONPath (RFC 9535)** — standardized query with filters, normalized paths, CTS conformance.
- ⬜ **MessagePack codec** — second binary target reusing the CBOR field reflection.
- ⬜ **SAX / event / pull API** — bounded-memory huge-document handling and transcoding.
- ⬜ **WebAssembly build + npm package** — run the parser (and CBOR) in the browser.
- ⬜ **Runtime CPU dispatch** — one binary that selects AVX-512/AVX2/NEON at load time.

## Tier 3 — Longer-term

- ⬜ **JSON Schema** (Draft-7 → 2020-12 subset) validation — OpenAPI / MCP alignment.
- ⬜ **Structural diff / patch generation** — compute a minimal RFC 6902 patch between two documents
  (real-time state sync).
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
