# qbuem-json Roadmap

> Status: **accepted** (2026-06-21). Built from a survey of the 2024–2026 JSON-library landscape
> (simdjson, yyjson, Glaze, reflect-cpp, jsoncons, serde, Boost.JSON) and current standards.
> The curated Tier 1→2→3 is **shipped through v1.13.0**; subsequent work follows the Direction below.

## Direction (2026-06-21): the JSON/CBOR layer for a high-performance C++ SaaS backend

The north star is **not** the game (that was a stack-validation throwaway) and **not** broad
Windows-OSS adoption. It is to be the serialization layer for a **C++ SaaS backend running on
Linux**, talking to a web (JS/TS) front end. Every prioritization below is judged against that.

What this locks in:

- **Windows / MSVC is a permanent non-goal**, not a gap. The server is Linux; nothing in the
  target stack runs C++ on Windows. For a solo maintainer, MSVC support is a *perpetual* tax
  (separate SIMD intrinsics, reflection-trick divergence, a Windows CI matrix, per-feature
  re-verification) with **zero** use-case value. Flip condition: a native Windows **C++** client
  (a web front end never triggers it). Until then, "GCC/Clang-only" is a deliberate advantage —
  it *enables* the macro-free reflection below that MSVC would block.
- **Optimize for:** known-schema DTO (de)serialization throughput (`fuse<T>`), untrusted-input
  safety at the API boundary (already a strength), low per-DTO boilerplate across a large API
  surface, and web-client interop (CBOR + WASM).

Post-v1.13.0 priorities (demand-driven, SaaS-backend lens):

| P | Item | Why it fits the SaaS-backend goal |
|---|------|-----------------------------------|
| **P0** | ✅ **Macro-free aggregate reflection** *(v1.14.0)* — plain aggregates serialize/deserialize (JSON + CBOR) with no `QBUEM_JSON_FIELDS`; macro/registration always wins; works for any linkage + non-constexpr members; ≤32 fields; `fuse<T>` still needs the macro | A SaaS has *many* request/response DTOs; per-type `QBUEM_JSON_FIELDS` was the main friction. GCC/Clang-only made `__PRETTY_FUNCTION__`-based field-name extraction viable **today** (no waiting for C++26 P2996). Closes the #1 ergonomic gap vs Glaze/reflect-cpp. |
| **P1** | ✅ **Honest head-to-head benchmarks + repositioning** *(v1.14.0 docs)* | Added an honest "where we lead vs where we don't" read to benchmarks.md (lead: struct decode / serialize / CBOR-vs-DOM / breadth; behind: simdjson/yyjson on raw schemaless parse). Repointed public copy from "games / HFT" → "C++ backend JSON/CBOR layer". Live CI dashboard stays the canonical number source. |
| **P1** | ⏸️ **Inline field validation** (range / length / required / enum) — **DEFERRED, needs a deliberate scope decision** | Genuine SaaS need, but the riskiest remaining item for over-engineering (a constraint set tends to balloon into a DSL: operators, custom validators, error aggregation, cross-field rules) and it is **not differentiated** (serde/reflect-cpp already do it; users can validate trivially after deserialize). Per the curation rules, not built speculatively — pick a tight scope deliberately before starting. |
| **P2** | 🟢 **Registry submission — recipes READY; submission needs the maintainer** | Conan recipe verified (`conan create` green at v1.14.0) and the vcpkg port pins the real v1.14.0 tarball SHA512. The remaining step is **outward-facing**: open PRs to the upstream ConanCenter / microsoft/vcpkg repos (requires a maintainer account). Submit when external adoption is wanted. |

Re-confirmed declines under the SaaS lens: full JSON Schema validator (interop documented; the
P1 inline validation is the SaaS-appropriate slice, a *different* thing), schemaless lazy cursor
(DTOs are known-schema → `fuse` covers it), incremental socket parse (HTTP bodies are
Content-Length-framed). Windows (above).

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
| 5 | ✅ **JSONPath (RFC 9535)** — structural selectors *(v1.8.0)* + **filter selectors `[?...]` shipped *(v1.13.0)*** (comparisons, existence, `&&`/`||`/`!`, `length()`/`count()`/`value()`; I-Regexp `match()`/`search()` declined — no regex engine in a single-header lib) | The biggest *query* completeness gap vs jsoncons/serde_json_path; first normative JSONPath (Feb 2024) → becoming table-stakes. | table-stakes | **L** |
| 8 | ✅ **WASM build + npm package** *(v1.9.0, bindings/wasm)* | Amplifies the cross-language pitch into the *browser* — validate / canonicalize / JSONPath, the things JS lacks natively. | differentiator | **S–M** |
| 7 | ✅ **SAX-style event visitor** *(v1.10.0, `visit` / `sax_parse`)* — event walk over the tape parser (not a second streaming parser) | Transcoding / inspection / folds without hand-navigating the DOM. | foundation | **M** |
| ~~9~~ | ❌ **Runtime CPU dispatch** — **DECLINED** (2026-06-21 curation) | Header-only consumers recompile (`-march=native` → optimal SIMD free); a multi-versioned dispatcher in the core hot path is high regression risk for a benefit only prebuilt artifacts see. Shipped a portable-vs-native build guide instead. | — | — |
| ~~6~~ | ❌ **MessagePack codec** — **DECLINED** (2026-06-21 curation) | Duplicative of the shipped CBOR codec (both binary-JSON); a second parallel codec is maintenance cost without clear differentiation. CBOR already covers the cross-language binary need. | — | — |

## Tier 3 — Longer-term / heavier

| # | Feature | Why | Effort | Status |
|---|---------|-----|--------|--------|
| 10 | ~~**JSON Schema (Draft-7 → 2020-12 subset)** validation~~ | Strongest *external* demand (OpenAPI 3.1/3.2, MCP default dialect) but heaviest. | **L–XL** | ❌ **DECLINED** (2026-06-21) — see below |
| 11 | ✅ **Structural diff / patch generation** *(v1.11.0, `qbuem::diff` + functional RFC 6902 `apply_patch`)* | Natural extension of our existing 6902; powers real-time **game state sync** (our use case). | **M–L** | done |
| 12 | ❌ **Schemaless On-Demand lazy cursor** — **DECLINED** | simdjson parity-chase; our flat-tape DOM + `fuse` already cover the known-schema case, and the iterator-invalidation semantics are a high regression-risk surface for a benefit our use case doesn't need. | — | declined |
| 13 | ⏸️ **Chunked / incremental (socket) parsing** — **DEFERRED** | WebSocket frames and framed CBOR are already message-delimited, so a whole message arrives before parse — the incremental need is largely already met. Revisit only if a real streaming workload appears. | **M** | deferred |
| 14 | ✅ **Packaging** *(v1.11.1 vcpkg + Conan)* + **JSONTestSuite conformance** *(283/283 mandatory, ASan+UBSan CI)* | Adoption + credibility; we are single-header + fuzzed already, this was the last mile. | **S** | done |
| 15 | ⏸️ **C++26 P2996 reflection backend** (opt-in, `#ifdef __cpp_reflection`) — **GATED** | Macro-free registration as an *additive fast-lane* (Glaze's dual-mode). **Not** a baseline — no stable compiler until ~2028; keep the macro as the portable default. | **L** | gated to ~2028 |

### #10 JSON Schema — DECLINED (build) / document interop instead

Validating against an external JSON Schema is a runtime concern, and three things
make shipping our own validator a poor fit:

1. **Not differentiated** — jsoncons (`jsonschema`), blaze, and valijson already
   do this well. A partial Draft-2020-12 subset would *copy for parity* (rule #2)
   while inviting "looks like Schema but isn't" frustration.
2. **Over-engineering risk** — even a subset is L–XL (`$ref` resolution,
   recursive schemas, format/pattern validators) and would noticeably bloat the
   single header (rule #3).
3. **Our use case doesn't need it** — for known C++ shapes, `fuse<T>` /
   `QBUEM_JSON_FIELDS` give *compile-time* structural validation: a missing or
   mistyped field is already a parse error, stronger than a runtime schema check.

→ Instead we **document interop**: validate dynamic/untrusted JSON at the trust
boundary with a dedicated library, then parse with qbuem-json for speed. See the
[Correctness guide → Schema validation](https://qbuem.com/qbuem-json/guide/correctness#json-schema-validation-interop).

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
- **Tier 3 = "depth bets."** Shipped the two that fit our identity (structural diff/patch +
  packaging/conformance). Declined the rest after curation: JSON Schema (not differentiated,
  over-engineering risk — document interop instead), the On-Demand lazy cursor (simdjson
  parity-chase), and incremental socket parsing (deferred — framing already covers it). P2996
  is the future, gated and additive — never the baseline.

## Status (2026-06-21): curated roadmap complete

Tier 1 + Tier 2 + the curated subset of Tier 3 are shipped through **v1.13.0**. Every remaining
candidate is consciously **declined / deferred / gated** above (not forgotten). The library is
feature-complete for its identity — *fastest + safest single-header C++20 JSON, dual-engine, with
a cross-language binary (CBOR) story*. Further work is demand-driven, not roadmap-driven.

### Sources
RFC 9535 JSONPath (Feb 2024) · RFC 8785 JCS · RFC 8949 §4.2 deterministic CBOR · JSON Schema
2020-12 (OpenAPI 3.1/3.2, MCP) · P2996 Reflection for C++26 (WG21 Sofia, Jun 2025; GCC 16 /
Bloomberg clang-p2996) · simdjson On-Demand + `iterate_many` · yyjson `yyjson_incr_read` ·
Glaze `format_error` / pure-reflection / BEVE · reflect-cpp annotations · serde container/field
attrs · nanobind · JSONTestSuite ("Parsing JSON is a Minefield").
