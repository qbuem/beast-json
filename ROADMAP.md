# qbuem-json Roadmap

> Status: **accepted** (2026-06-21). Built from a survey of the 2024–2026 JSON-library landscape
> (simdjson, yyjson, Glaze, reflect-cpp, jsoncons, serde, Boost.JSON) and current standards.
> Execution is **sequential: Tier 1 → 2 → 3.**

## Guiding principles

Every candidate feature is admitted only if it passes all three:

1. **General-purpose** — it makes the library *broadly* more useful (not a one-customer hook).
2. **Differentiated** — it strengthens an axis where we lead or are under-served by competitors,
   rather than copying for parity's sake.
3. **No over-engineering** — ship the smallest version that delivers the value; a *subset* that is
   correct and fast beats a maximal feature that bloats the header. Single-header, zero-dep, and the
   performance/safety bar are non-negotiable constraints.

Anything that fails these is **declined** (see the list at the bottom) — including several
aspirational items from the old vision.

## Where we are (v1.2.0)

**Shipped:** dual-engine core (flat-tape SIMD DOM + zero-tape `fuse` struct mapping) ·
AVX-512 / NEON / SVE / SWAR · three-stage float parse (Eisel-Lemire → Russ Cox → strtod) +
Schubfach serialize · **CBOR codec** (RFC 8949, positional decode fast-path, generics via
`QBUEM_JSON_FIELDS_TPL`) · JSON Pointer (6901) · JSON Patch (6902) · Merge Patch (7396) ·
relaxed/comments parse mode · `rfc8259::validate` (well-formedness) · `std::pmr` · single-header ·
bindings (Python/nanobind, Rust/cxx, Go/cgo) · multi-arch ASan/UBSan/TSan + libFuzzer CI.

**Our identity to defend:** fastest + safest *single-header* C++20 JSON, dual-engine, with a
*cross-language binary* (CBOR) story. The roadmap closes "feature-completeness" gaps that make us
look partial next to jsoncons/Glaze, **and** doubles down on the binary + cross-language
differentiators where few competitors play.

---

## Tier 1 — Next (v1.3.x): high ROI, mostly S/M

| # | Feature | Why | Class | Effort |
|---|---------|-----|-------|--------|
| 1 | **Rich error context** — line/column + JSON-Pointer path + caret snippet (extend `parse_error`, today byte-`offset()` only) | Best effort:payoff DX gap; the bar set by Glaze `format_error` / serde_json. Cheap trust. | table-stakes | **S–M** |
| 2 | **serde-style field attributes** — enum⇄string, `rename`/`alias`, `skip`, `default` (extend the `QBUEM_JSON_FIELDS` FOR_EACH; hashes already drive dispatch → zero hot-path cost) | Closes the DX gap vs serde/reflect-cpp; all doable in **C++20 today**. | table-stakes | **S–M** |
| 3 | **NDJSON / JSON Lines** stream (`parse_many`-style, bounded memory) | Dominant format for logs / ML / LLM output / pipelines; multi-GB in constant memory. Aligns with our framed-CBOR pipeline. | table-stakes | **M** |
| 4 | **Deterministic/Canonical CBOR (§4.2)** + **JSON Canonicalization (RFC 8785 / JCS)** | Differentiator: enables signing/hashing; synergizes with our CBOR. **Few C++ libs ship JCS.** We already emit shortest ints + definite maps — mostly need key sorting. | differentiator | **M** |

## Tier 2 — Mid-term (v1.4–1.5): strategic, bigger

| # | Feature | Why | Class | Effort |
|---|---------|-----|-------|--------|
| 5 | **JSONPath (RFC 9535)** — query, filters, normalized paths, CTS conformance | The biggest *query* completeness gap vs jsoncons/serde_json_path; first normative JSONPath (Feb 2024) → becoming table-stakes. | table-stakes | **L** |
| 6 | **MessagePack codec** (reuse the same field reflection as CBOR) | Broadens binary reach into Redis/msgpack ecosystems; near-free given the CBOR machinery. | adjacent | **M** |
| 7 | **SAX / event / pull API** over the existing tokenizer | Foundation for bounded-memory huge-doc handling + transcoding without a DOM. | foundation | **M** |
| 8 | **WASM build + npm package** (header-only → cheap; ship scalar fallback) | Amplifies "CBOR decodes in any language" into the *browser* — directly serves the cross-language pitch. | differentiator | **S–M** |
| 9 | **Runtime CPU dispatch** (one binary runs on any x86 level; we are `-march=native` static today) | Matters for prebuilt bindings / WASM / packaged distribution. simdjson's model. | infra | **M** |

## Tier 3 — Longer-term / heavier

| # | Feature | Why | Effort |
|---|---------|-----|--------|
| 10 | **JSON Schema (Draft-7 → 2020-12 subset)** validation | Strongest *external* demand (OpenAPI 3.1/3.2, MCP default dialect) but heaviest; ship a subset or document blaze/jsoncons interop. | **L–XL** |
| 11 | **Structural diff / patch generation** (compute a minimal 6902 patch between two docs) | Natural extension of our existing 6902; powers real-time **game state sync** (our use case). | **M–L** |
| 12 | **Schemaless On-Demand lazy cursor** (parse only what you touch) | simdjson's headline model; our `fuse` already covers the *known-schema* case, this is the schemaless gap. Subtle iterator-invalidation semantics. | **L** |
| 13 | **Chunked / incremental (socket) parsing** — resumable `write_some`-style feeding | Parse straight off a socket without full buffering; pairs with framed CBOR. | **M** |
| 14 | **Packaging**: vcpkg + Conan registry entries; explicit **JSONTestSuite** conformance badge | Adoption + credibility; we are single-header + fuzzed already, this is the last mile. | **S** |
| 15 | **C++26 P2996 reflection backend** (opt-in, `#ifdef __cpp_reflection`) | Macro-free registration as an *additive fast-lane* (Glaze's dual-mode). **Not** a baseline — no stable compiler until ~2028; keep the macro as the portable default. | **L** |

## Defer / Decline (low ROI or speculative)

- **Coroutine/async parsing, intra-document multithreading, constexpr JSON** — weak demand, L / high-risk.
- **JMESPath / JSONata / jq** embedding — runtime-heavy, niche; JSONPath covers the query need.
- **BSON, Amazon Ion, Apache Arrow/columnar, FlatBuffers/Cap'n Proto** — niche for our game/cross-language use case.
- **Retire from the old vision** as written (over-ambitious, low ROI): *"Nexus Protocol Shifting (JSON↔SBE/FIX)"*, *"Nexus IDL Inference (extract Protobuf/FlatBuffers IDL from C++)"*, *"Nexus Codegen"*. (SVE, bindings, and pmr from the old vision are **already done**.)

---

## Strategic shape

- **Tier 1 = "look complete + lean into binary."** Quick DX wins (errors, attributes) + NDJSON
  remove the "partial library" impression; canonical CBOR/JCS deepens the one axis (binary +
  cross-language) where we already lead.
- **Tier 2 = "feature parity + reach."** JSONPath + SAX close the last big functional gaps;
  WASM + MessagePack + runtime dispatch extend reach without diluting the core.
- **Tier 3 = "depth bets."** Schema and the On-Demand cursor are large; take them on once the
  Tier 1/2 base is broad. P2996 is the future, gated and additive — never the baseline.

### Sources
RFC 9535 JSONPath (Feb 2024) · RFC 8785 JCS · RFC 8949 §4.2 deterministic CBOR · JSON Schema
2020-12 (OpenAPI 3.1/3.2, MCP) · P2996 Reflection for C++26 (WG21 Sofia, Jun 2025; GCC 16 /
Bloomberg clang-p2996) · simdjson On-Demand + `iterate_many` · yyjson `yyjson_incr_read` ·
Glaze `format_error` / pure-reflection / BEVE · reflect-cpp annotations · serde container/field
attrs · nanobind · JSONTestSuite ("Parsing JSON is a Minefield").
