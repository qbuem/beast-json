# Changelog

All notable changes to **qbuem-json** are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Scope: Linux (x86_64 / aarch64) and macOS (Apple Silicon), GCC and Clang.
Windows/MSVC is intentionally unsupported by design.

## [Unreleased]

### Added
- **Macro-free `fuse<T>` for aggregates.** The zero-tape Nexus engine now works on a
  plain aggregate with no `QBUEM_JSON_FIELDS` — including generic (template) and nested
  aggregates — completing the macro-free story (already true for `read` / `write` /
  `read_strict` / CBOR). Key→field routing uses a reflected compile-time-hash ladder; the
  macro's compile-time `switch` stays the peak-dispatch path for very wide hot-path
  structs (~1.5× on a realistic DTO, growing with field count). GCC/Clang, plain
  aggregate, ≤32 fields. A `QBUEM_JSON_FIELDS` registration always wins when present.

### Fixed
- **Rust binding benchmark produced no numbers in CI.** `bindings/rust/build.rs`
  was matched by an over-broad `build*` `.gitignore` rule and had never been
  committed, so CI built the crate without its cxx-bridge build script — the C++
  shim was never compiled and the bench failed to link
  (`undefined symbol: qbuem$rust$cxxbridge1$…`). The `.gitignore` rule is now
  `build*/` (directories only) and `build.rs` is tracked. The shim is linked with
  `+whole-archive` so a single-pass linker with `--gc-sections` cannot drop the
  bridge trampolines. Rust parse/serialize numbers now publish on all three
  benchmark platforms.

### Changed
- **CI (`benchmark.yml`):** commit Rust `Cargo.lock` and run `cargo bench --locked`
  for reproducible binding benchmarks; trigger the benchmark on `bindings/**`
  changes; only commit aggregated results back to `main` from a `push` to `main`
  (prevents a branch `workflow_dispatch` from pushing to `main`).

## [1.16.0] — 2026-06-22

### Fixed
- **RFC conformance hardening, verified against the official suites.**
  - **JSONPath (RFC 9535):** now passes 647/647 of the
    jsonpath-compliance-test-suite (`match()`/`search()` I-Regexp excluded by
    design). Fixes in the parser: `\uXXXX`/surrogate decoding and rejection of
    control chars and invalid surrogates in quoted names; strict integer and
    number grammar; member-name first-character rules; no leading/trailing
    whitespace; decoded-key matching (`$["\n"]` now resolves correctly).
  - **Canonicalization (RFC 8785):** byte-exact on all six official JCS vectors —
    UTF-16 code-unit key ordering and the ECMAScript `Number` text format
    (`1e+30`, not `1E30`).
- CI gained a conformance job that fetches JSONTestSuite, the JSONPath CTS, and the
  JCS vectors at pinned commits and runs them under ASan + UBSan.

## [1.15.0] — 2026-06-22

### Added
- `qbuem::read_strict<T>()` — like `read<T>()` but throws when a required
  (non-`optional`) field is missing; checked recursively for nested structs.
  Aimed at validating backend request DTOs.

## [1.14.0] — 2026-06-21

### Added
- **Macro-free aggregate reflection (GCC/Clang).** Plain aggregates serialize to
  JSON and CBOR with no `QBUEM_JSON_FIELDS` registration. The macro still wins
  when present, and `fuse<T>` continues to require it.

## [1.13.0] — 2026-06-21

### Added
- **JSONPath filter selectors `[?…]` (RFC 9535 §2.3.5):** comparisons, existence
  tests, `&&`/`||`/`!` with parentheses, singular-query operands, and the
  `length()`/`count()`/`value()` functions. Filter nesting is depth-capped (512)
  as a DoS guard. `match()`/`search()` are intentionally not supported (no regex
  engine in a single header).

## [1.12.0] — 2026-06-21

### Changed
- **API hygiene.** Internal helpers and the JSONPath parser moved out of the public
  `qbuem::` namespace into `qbuem::json::detail`. Added buffer-append `_to`
  overloads (`canonicalize_to` / `diff_to` / `apply_patch_to`) and a
  `qbuem::jsonpath` alias for `query`. No behavioral change.

## [1.11.2] — 2026-06-21

### Fixed
- `value_equal` compared numbers as `double`, equating e.g. `2^53` and `2^53+1`
  (so `diff` missed large-integer-ID changes and the `test` op spuriously passed);
  now int64-exact with a double fallback.
- diff/`value_equal` matched object keys by raw escaped bytes, producing false
  remove+add for differently-escaped identical keys; added decoded-key matching.
- JSONPath index parsing could overflow; indices are now range-checked per RFC 9535.

## [1.11.1] — 2026-06-21

### Fixed
- Packaging: `find_package(qbuem_json CONFIG)` now works; added a Conan recipe and
  a vcpkg port.

## [1.11.0] — 2026-06-21

### Added
- `qbuem::diff()` (structural JSON diff) and a complete functional **RFC 6902**
  applier, `qbuem::apply_patch()`.

## [1.10.0] — 2026-06-21

### Added
- **SAX-style event visitor:** `qbuem::visit` / `sax_parse` with a `sax_handler`
  base. A static-dispatch event walk over the existing tape — not a second parser.

## [1.9.0] — 2026-06-21

### Added
- **WebAssembly module (`bindings/wasm`, Emscripten/Embind):** validate, minify,
  prettify, canonicalize, and JSONPath query in the browser and Node, exposing the
  same conformant core.

## [1.8.0] — 2026-06-21

### Added
- **JSONPath query (`qbuem::query`, RFC 9535 structural selectors):** root, member,
  index, wildcard, descendant `..`, slice, and union. Filters arrived in 1.13.0.

## [1.7.0] — 2026-06-21

### Added
- **Canonical JSON (`qbuem::canonicalize`, RFC 8785-style):** sorted keys, shortest
  number forms, `-0` → `0`. Completes roadmap Tier 1.

## [1.6.0] — 2026-06-21

### Added
- **NDJSON / JSON Lines:** `qbuem::read_lines` / `write_lines` with bounded memory
  (one reused `Document`).

### Fixed
- Null-document dereference in the array/object iterators on a malformed key with
  no value (`{"tags"}`); invalid `Value`s now iterate as empty per contract.

## [1.5.0] — 2026-06-21

### Added
- Field rename `(member, "jsonKey")` and skip-by-omission. The bare-field code path
  is byte-identical to before.

## [1.4.0] — 2026-06-21

### Added
- **Enum support.** Serialized as the underlying integer by default across all
  engines; opt into value-name strings with `QBUEM_JSON_ENUM(E, …)`.

## [1.3.0] — 2026-06-21

### Added
- **Rich parse-error context:** `parse_error::line()` / `column()` and
  `qbuem::format_error(e, src)` with a caret pointing at the offending byte.

## [1.2.0] — 2026-06-19

### Added
- `QBUEM_JSON_FIELDS_TPL` — register a class template once for all instantiations
  (DOM read/write, fuse, CBOR). Measured zero runtime overhead vs the concrete macro.

## [1.1.0] – [1.1.4] — 2026-06-19

### Added
- **CBOR (RFC 8949) binary codec.** `qbuem::cbor::encode<T>()` / `decode<T>()` drive
  off the same `QBUEM_JSON_FIELDS` list; the bytes decode in any language (e.g. JS
  `cbor-x`). The decoder is bounds-checked and depth-bounded for untrusted input.

### Changed
- CBOR hot-path performance (1.1.1–1.1.4): definite-length struct maps, compile-time
  key blobs and bulk writes on encode, `read_head` hot/cold split and a positional
  decode fast path. Encode/decode end up faster than the JSON path.

## [1.0.8] — 2026-06-16

### Added
- Typed `qbuem::parse_error` (derives `std::runtime_error`) with `offset()`.
- `decoded()` accessor; `decoded()` always returns valid UTF-8 (lone surrogates →
  U+FFFD).

### Fixed
- **Untrusted-input hardening.** Depth-overflow stack smashing, validator recursion
  DoS, oversized-token truncation, `dump()` heap overflow, exponential pretty-print
  blow-up, Nexus out-of-bounds / infinite-loop OOM, narrow-int range checks, full
  `uint64` support, the Nexus hash-only field-spoofing gap, and a zero-copy
  use-after-free foot-gun (the rvalue-`std::string` parse overloads are deleted).
- **Strict UTF-8 validation** in `rfc8259::validate` (rejects overlong forms,
  surrogates, out-of-range, truncated, and lone continuation bytes per Unicode
  Table 3-7). Relaxed mode stays byte-transparent.

### Changed
- CI: sanitizer matrix (ASan/UBSan/TSan) and a libFuzzer gate.

## [1.0.0] – [1.0.7] — 2026-03

### Added
- Initial public releases of the single-header, zero-dependency C++20 JSON library:
  dual-engine SIMD DOM parser plus zero-tape struct mapping via `QBUEM_JSON_FIELDS`,
  RFC 8259 compliance, and IEEE 754 round-tripping.

[Unreleased]: https://github.com/qbuem/qbuem-json/compare/v1.16.0...HEAD
[1.16.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.16.0
[1.15.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.15.0
[1.14.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.14.0
[1.13.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.13.0
[1.12.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.12.0
[1.11.2]: https://github.com/qbuem/qbuem-json/releases/tag/v1.11.2
[1.11.1]: https://github.com/qbuem/qbuem-json/releases/tag/v1.11.1
[1.11.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.11.0
[1.10.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.10.0
[1.9.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.9.0
[1.8.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.8.0
[1.7.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.7.0
[1.6.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.6.0
[1.5.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.5.0
[1.4.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.4.0
[1.3.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.3.0
[1.2.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.2.0
[1.1.4]: https://github.com/qbuem/qbuem-json/releases/tag/v1.1.4
[1.1.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.1.0
[1.0.8]: https://github.com/qbuem/qbuem-json/releases/tag/v1.0.8
[1.0.7]: https://github.com/qbuem/qbuem-json/releases/tag/v1.0.7
[1.0.0]: https://github.com/qbuem/qbuem-json/releases/tag/v1.0.0
