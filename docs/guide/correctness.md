# Correctness & Safety

<div style="display: flex; flex-wrap: wrap; gap: 0.4rem; margin: 1rem 0 1.25rem; line-height: 1.9;">
  <a href="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/ci.yml/badge.svg" alt="CI" /></a>
  <a href="https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml/badge.svg" alt="Sanitizers (ASan · UBSan · TSan)" /></a>
  <a href="https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml/badge.svg" alt="Benchmark CI" /></a>
  <a href="https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml"><img src="https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml/badge.svg" alt="CodeQL" /></a>
  <img src="https://img.shields.io/badge/tests-675%20passing-brightgreen" alt="675 tests passing" />
  <img src="https://img.shields.io/badge/fuzz-17%20libFuzzer%20targets-orange" alt="17 libFuzzer targets" />
  <img src="https://img.shields.io/badge/RFC%208259-compliant-brightgreen" alt="RFC 8259" />
  <img src="https://img.shields.io/badge/RFC%206901-JSON%20Pointer-brightgreen" alt="RFC 6901" />
  <img src="https://img.shields.io/badge/RFC%206902-JSON%20Patch-brightgreen" alt="RFC 6902" />
  <img src="https://img.shields.io/badge/IEEE%20754-round--trip-brightgreen" alt="IEEE 754 round-trip" />
</div>

<!-- Summary stats -->
<div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(110px, 1fr)); gap: 0.75rem; background: linear-gradient(135deg, #f0f4ff, #e8f0ff); border: 1px solid #c0d0ff; border-radius: 12px; padding: 1.25rem 1.5rem; margin: 0 0 2rem; text-align: center;">
  <div>
    <div style="font-size: 1.9rem; font-weight: 800; color: #1e2e5c; line-height: 1.1;">675</div>
    <div style="font-size: 0.78rem; color: #555; margin-top: 0.2rem;">tests passing</div>
  </div>
  <div>
    <div style="font-size: 1.9rem; font-weight: 800; color: #1e2e5c; line-height: 1.1;">73</div>
    <div style="font-size: 0.78rem; color: #555; margin-top: 0.2rem;">RFC 8259 cases</div>
  </div>
  <div>
    <div style="font-size: 1.9rem; font-weight: 800; color: #1e2e5c; line-height: 1.1;">10</div>
    <div style="font-size: 0.78rem; color: #555; margin-top: 0.2rem;">CI configurations</div>
  </div>
  <div>
    <div style="font-size: 1.9rem; font-weight: 800; color: #1e2e5c; line-height: 1.1;">3×</div>
    <div style="font-size: 0.78rem; color: #555; margin-top: 0.2rem;">sanitizers</div>
  </div>
  <div>
    <div style="font-size: 1.9rem; font-weight: 800; color: #1e2e5c; line-height: 1.1;">17</div>
    <div style="font-size: 0.78rem; color: #555; margin-top: 0.2rem;">fuzz targets</div>
  </div>
</div>

This page documents the concrete testing and verification infrastructure behind
qbuem-json.  Every claim here is backed by a CI job you can inspect and
reproduce locally.

---

## At a glance

| Signal | Status | Details |
|:---|:---:|:---|
| Total tests | **675** | 29 test files, ~7,700 lines |
| RFC 8259 compliance tests | **73** | 23 accept · 24 reject · 8 impl-defined · 3 roundtrip · 3 API |
| RFC 6901 JSON Pointer | ✅ | Pointer navigation + edge cases |
| RFC 6902 JSON Patch | ✅ | add / remove / replace / move / copy / test ops, transactional rollback |
| AddressSanitizer (ASan) | ✅ CI | [sanitizers.yml](https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml) |
| UndefinedBehaviorSanitizer (UBSan) | ✅ CI | [sanitizers.yml](https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml) |
| ThreadSanitizer (TSan) | ✅ CI | [sanitizers.yml](https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml) |
| Fuzz testing | ✅ | 17 libFuzzer targets · **64.02% branch coverage** |
| JSONTestSuite conformance | ✅ CI | **283/283** mandatory cases (95 accept · 188 reject) under ASan+UBSan |
| IEEE 754 round-trip | ✅ | All 64-bit doubles; parsing: `eisel_lemire_f64` (~98.8 %) → `russ_cox_uscale_f64` (~1.2 %) → `strtod` (subnormals); serialization: Schubfach / `qj_nc::f64_to_dec` |
| CodeQL static analysis | ✅ CI weekly | security-extended query suite |
| Multi-platform CI | ✅ [10 configs](https://github.com/qbuem/qbuem-json/blob/main/.github/workflows/ci.yml) | GCC 13/14 · Clang 18 · Apple Clang · x86_64 · aarch64 · Apple Silicon |

---

## RFC 8259 compliance

qbuem-json ships 73 compliance test cases in `tests/test_compliance.cpp`.
Test suite names map to JSONTestSuite semantics:

| Suite | Meaning | Count |
|:---|:---|---:|
| `RFC8259_Accept` | **Must accept** — valid JSON; parser must not throw | 23 |
| `RFC8259_Reject` | **Must reject** — invalid JSON; `parse_strict()` must throw | 24 |
| `RFC8259_ImplDefined` | **Implementation-defined** — we document our choice | 8 |
| `RFC8259_Roundtrip` | Parse → serialize → re-parse equality | 3 |
| `RFC8259_API` | API-level compliance (lenient vs strict mode) | 3 |
| `RFC6901_Pointer` | JSON Pointer navigation and edge cases | 3 |
| `RFC6902_Patch` | JSON Patch operations + transactional rollback | 9 |
| **Total** | | **73** |

### What is tested

- All JSON value types at the root level (`null`, `true`, `false`, numbers,
  strings, arrays, objects)
- Number forms: zero, negative zero, integers, floats, exponents, edge-case
  exponents (`1.23e+456`)
- String escapes: `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`,
  `\uXXXX`, surrogate pairs
- Structural constraints: empty object, empty array, deeply nested structures,
  trailing commas (rejected in strict mode)
- RFC 6901 JSON Pointer: simple paths, `/`, `~0`/`~1` escapes, array indexing,
  end-of-array token `-`, leading-zero rejection
- RFC 6902 JSON Patch: all six operations plus transactional rollback (a
  mid-patch failure must leave the document unchanged)

### Lenient vs strict mode

The library ships two parsers:

| Function | Mode | Behaviour |
|:---|:---|:---|
| `qbuem::parse()` | Lenient | Accepts relaxed JSON (comments, trailing commas) |
| `qbuem::parse_strict()` | RFC 8259 strict | Rejects anything outside the spec |

All `n_` tests use `parse_strict()`.  The lenient parser's extensions are
explicitly documented so you can opt in intentionally.

### UTF-8 well-formedness (RFC 8259 §8.1)

`parse_strict()` / `rfc8259::validate()` validate that string content is
**well-formed UTF-8**, not just syntactically valid JSON. Malformed byte
sequences are rejected against the Unicode "well-formed UTF-8" table (Table 3-7):

| Rejected | Example bytes |
|:---|:---|
| Overlong encodings | `C0 AF` (a non-shortest form of `/` — a filter-bypass vector) |
| UTF-8-encoded surrogates | `ED A0 80` (U+D800) |
| Code points > U+10FFFF | `F4 90 80 80` |
| Lone continuation bytes | `80` |
| Truncated sequences | `E2 9C` (3-byte lead, one byte short) |

The **lenient** `parse()` is byte-transparent (it does not UTF-8-validate) so
`as<std::string_view>()` can stay zero-copy. A lone surrogate written as a
`\uXXXX` *escape* is accepted (RFC implementation-defined); `decoded()` then
replaces it with U+FFFD so decoded output is always valid UTF-8.

### Duplicate object keys

RFC 8259 §4 says names *should* (not *must*) be unique, so duplicates are valid
JSON and are accepted. Resolution is deterministic but engine-specific: the DOM
(`operator[]`, `read<T>`) is **first-wins**; Nexus (`fuse<T>`) is **last-wins**
(matching JavaScript / Python / Go). Reject or canonicalize upstream if duplicate
keys are security-relevant.

---

## JSONTestSuite conformance

Beyond the hand-written cases above, qbuem-json is run against the full
[**JSONTestSuite**](https://github.com/nst/JSONTestSuite) corpus — Nicolas
Seriot's *"Parsing JSON is a Minefield"* set, the de-facto adversarial benchmark
every serious JSON parser is measured against (318 files probing number edges,
UTF-8 traps, structural limits, and whitespace corner cases).

The corpus splits cases by required behaviour, and `parse_strict()` is **100% on
both mandatory categories**:

| Category | Meaning | Result |
|:---|:---|:---|
| `y_` | **Must accept** — well-formed JSON | **95 / 95** ✅ |
| `n_` | **Must reject** — malformed JSON | **188 / 188** ✅ |
| `i_` | Implementation-defined — either outcome is RFC-legal | 35, profiled below |

Passing all 188 `n_` cases is the load-bearing result: it means the strict
parser never *accepts* invalid JSON, the dangerous direction when the input is
untrusted. The run executes under **ASan + UBSan**, so the 318 adversarial
inputs simultaneously prove memory safety.

### Implementation-defined profile

The 35 `i_` cases have no single correct answer, so we **freeze our documented
choices** as a regression guard (`tests/test_jsontestsuite.cpp`) — a code change
that flips any one of them fails CI and must be reviewed deliberately. The theme
is coherent:

| We **accept** (21) | We **reject** (14) |
|:---|:---|
| Numeric overflow / underflow → ±inf / 0 per IEEE 754 (`1e400`, huge exponents, oversized ints) | Every **malformed UTF-8/UTF-16 byte** sequence — invalid, overlong, truncated, lone continuation, ISO-Latin-1, UTF-16 without BOM |
| Lone / inverted `\uXXXX` surrogate *escapes* (decoded to U+FFFD) | A leading UTF-8 BOM |
| Nesting up to the parser's depth cap (`i_structure_500_nested_arrays`) | |

In short: **strict on encoding-level validity, lenient on `\u`-escape surrogate
pairing and numeric magnitude.**

### Running it yourself

The corpus is not vendored (it carries its own licence and would bloat the
repo); `test_jsontestsuite` skips unless you point it at a checkout:

```bash
git clone https://github.com/nst/JSONTestSuite.git
QBUEM_JSONTESTSUITE_DIR=$PWD/JSONTestSuite ctest --test-dir build -R JSONTestSuite
```

CI fetches it at a pinned commit and runs the suite on every push and pull
request (the `conformance` job in [ci.yml](https://github.com/qbuem/qbuem-json/blob/main/.github/workflows/ci.yml)).

---

## Standards & RFC conformance

Every RFC the library claims is verified against an **official external test
suite** where one exists — not just internal unit tests. The conformance job runs
these on every push, under ASan+UBSan:

| RFC | What | How it's verified | Result |
|:---|:---|:---|:---|
| **RFC 8259** | JSON syntax | [JSONTestSuite](https://github.com/nst/JSONTestSuite) (318 cases) | **283/283** mandatory (95 accept · 188 reject) |
| **RFC 9535** | JSONPath | [JSONPath Compliance Test Suite](https://github.com/jsonpath-standard/jsonpath-compliance-test-suite) (`cts.json`) | **all** applicable tests pass *(the I-Regexp `match()`/`search()` functions are the only intentional exclusion — they throw)* |
| **RFC 8785** | JSON Canonicalization (JCS) | [cyberphone/json-canonicalization](https://github.com/cyberphone/json-canonicalization) vectors | **byte-exact** on all 6 vectors (UTF-16 key ordering + ECMAScript number formatting) |
| **RFC 8949** | CBOR | round-trip + decode of `qbuem::cbor::encode` output by a third-party decoder (`cbor2`) | wire format valid & round-trips *(scoped to the struct/value data model — not a general CBOR parser with tags/bignums)* |
| **RFC 6901** | JSON Pointer | unit tests over the RFC's own examples (`test_compliance`) | pass |
| **RFC 6902** | JSON Patch | all six ops + transactional rollback (`test_compliance`, `test_diff`) | pass |
| **RFC 7396** | JSON Merge Patch | RFC appendix examples | pass |
| **IEEE 754** | float round-trip | all 64-bit doubles via the three-stage parser | exact |

The external suites are fetched at pinned commits and locked by
`tests/test_rfc_conformance.cpp` (skips locally unless `QBUEM_JSONPATH_CTS` /
`QBUEM_JCS_DIR` point at checkouts; always run in CI), so a future change that
breaks conformance fails the build.

---

## JSON Schema validation (interop)

qbuem-json **does not ship a JSON Schema validator**, and that is a deliberate
scope decision, not a gap:

- **For known C++ shapes, you don't need one.** `QBUEM_JSON_FIELDS` + `read<T>()`
  / `fuse<T>` give you *compile-time* structural validation: a missing or
  mistyped field is already a typed parse error. That is stronger and faster than
  a runtime schema check, with zero schema document to maintain.

  ```cpp
  struct CreateUser { std::string name; int age; };
  QBUEM_JSON_FIELDS(CreateUser, name, age)

  auto r = qbuem::read<CreateUser>(body);   // wrong type / missing field → r is an error
  if (!r) return reject(r.error());          // no JSON Schema needed
  ```

- **For dynamic JSON against an *external* schema** (OpenAPI 3.1, the MCP default
  dialect, etc.), validate at the trust boundary with a dedicated, fully
  Draft-2020-12-compliant library — [jsoncons
  `jsonschema`](https://github.com/danielaparker/jsoncons),
  [blaze](https://github.com/sourcemeta/blaze), or
  [valijson](https://github.com/tristanpenman/valijson) — then hand the accepted
  payload to qbuem-json for fast parsing / mapping:

  ```cpp
  // 1) validate untrusted text against the schema (dedicated lib, once, at the edge)
  if (!schema_validator.is_valid(untrusted_text)) return reject();
  // 2) parse the now-trusted bytes with qbuem-json (fast path)
  auto doc = qbuem::read<RequestDto>(untrusted_text);
  ```

A single-header JSON parser and a Draft-2020-12 schema engine (with `$ref`
resolution, recursive schemas, and format/pattern validators) are different
tools; keeping them separate keeps this library single-header, zero-dependency,
and fast, while letting you pair it with a best-in-class validator when you
genuinely need runtime schema enforcement.

---

## Memory safety — sanitizers

Sanitizer CI runs on every push and pull request to `main` when `include/`,
`tests/`, or `CMakeLists.txt` changes.

| Job | Tool | What it catches |
|:---|:---|:---|
| `asan-ubsan` | Clang 18 ASan + UBSan | Heap/stack overflows · use-after-free · use-after-scope · double-free · memory leaks · signed integer overflow · null pointer dereference · misaligned access · out-of-bounds array indexing |
| `tsan` | Clang 18 TSan | Data races · lock-order inversions · use of uninitialised mutexes |

> ASan and TSan are incompatible and run in separate jobs, as required by the
> LLVM toolchain.

**Run locally:**

```bash
# ASan + UBSan
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DQBUEM_JSON_BUILD_TESTS=ON \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build -j$(nproc)
ASAN_OPTIONS="halt_on_error=1:detect_leaks=1" \
UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
ctest --test-dir build --output-on-failure

# TSan
cmake -B build_tsan -DCMAKE_BUILD_TYPE=Debug \
  -DQBUEM_JSON_BUILD_TESTS=ON \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer"
cmake --build build_tsan -j$(nproc)
TSAN_OPTIONS="halt_on_error=1" \
ctest --test-dir build_tsan --output-on-failure
```

---

## Fuzz testing

Seventeen [libFuzzer](https://llvm.org/docs/LibFuzzer.html) targets are maintained
in `fuzz/`, providing exhaustive coverage of core parsing, struct mapping, and
every higher-level surface (CBOR, JSONPath, NDJSON, SAX, diff/patch, reflection):

| Target | Input | Functionality Tested |
|:---|:---|:---|
| `fuzz_dom` | Arbitrary bytes | DOM parser — crash/hang on malformed input |
| `fuzz_parse` | Arbitrary bytes | High-level `parse()` + full accessor surface |
| `fuzz_rfc8259` | Arbitrary bytes | Strict parser — RFC 8259 acceptance/rejection consistency |
| `fuzz_float` | Arbitrary bytes | Three-stage float parsing (Eisel-Lemire, Russ Cox) |
| `fuzz_direct` | Arbitrary bytes | DirectParser & key mapping (zero-tape) |
| `fuzz_nexus` | Arbitrary bytes | Nexus Fusion struct mapping |
| `fuzz_api_stress` | Arbitrary bytes | DOM mutation, SafeValue, runtime API safety |
| `fuzz_roundtrip` | Arbitrary bytes | Parse → serialize → re-parse identity |
| `fuzz_diff` | Arbitrary bytes | Differential testing vs `nlohmann/json` |
| `fuzz_pmr` | Arbitrary bytes | Memory safety with `std::pmr` allocators |
| `fuzz_patch` | Arbitrary bytes | JSON Pointer & in-place mutation |
| `fuzz_cbor` | Arbitrary bytes | CBOR codec — decode hostile bytes + round-trip |
| `fuzz_ndjson` | Arbitrary bytes | NDJSON line splitter + sub-view parse safety |
| `fuzz_jsonpath` | Arbitrary bytes | JSONPath (RFC 9535) parser + evaluator, incl. filters |
| `fuzz_sax` | Arbitrary bytes | SAX event visitor recursion + handler dispatch |
| `fuzz_apply_patch` | Arbitrary bytes | RFC 6902 functional applier + diff generation |
| `fuzz_reflect` | Arbitrary bytes | Macro-free aggregate reflection: JSON read + CBOR decode |

### Coverage Results

We achieved high-quality validation with a focus on logic density:
- **Branch Coverage**: **64.02%**
- **Region Coverage**: 62.00%
- **Crashes Discovered**: 0 in latest 1.9M+ run (all previous bugs fixed)

A seed corpus in `fuzz/corpus/` seeds each target with valid JSON samples from
the benchmark suite and specialized edge cases (Scientific notation, Unicode surrogate pairs, deep nesting).

**Build and run all fuzzing targets with coverage:**

```bash
# Using the automated coverage script
./fuzz/gen_coverage.sh
```

**Run a specific fuzz target locally:**

```bash
cmake -B build_fuzz \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DQBUEM_JSON_BUILD_FUZZ=ON \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,fuzzer"
cmake --build build_fuzz -j$(nproc)

# Run fuzz_dom with the seed corpus:
./build_fuzz/fuzz/fuzz_dom fuzz/corpus
```

---

## IEEE 754 floating-point correctness

Every `double` parsed by qbuem-json and every `double` serialised by qbuem-json
satisfies the **shortest round-trip** guarantee:

> `parse(serialize(x)) == x` for all finite `double` values.

This is enforced by a three-stage parsing pipeline and one serialization algorithm, all implemented in `include/qbuem_json/qbuem_json.hpp`:

### Parsing — three-stage pipeline

| Stage | Function | Coverage | Algorithm |
|:---|:---|:---|:---|
| 1 | `eisel_lemire_f64()` | ~98.8 % | 128-bit multiplication of normalized mantissa × table entry; second-multiply refinement if guard bits ambiguous. Returns false if still unresolved. |
| 2 | `russ_cox_uscale_f64()` | ~1.2 % | Ceiling of table high word: `ph_ceil = ph + (pl ≠ 0)`. The ceiling bias guarantees `lower ≠ 0` whenever rounding is genuinely undecidable — no return-false path. Proved by [Ivy](https://research.swtch.com/fp-proof). |
| 3 | `std::strtod` | < 0.01 % | Subnormals (biased exponent ≤ 0) and >19-digit mantissas only. |

Both fast-path functions share the pre-built `pow10_sig_table_128` (128-bit powers of 10 from 10⁻³⁴³ to 10³²⁴) that the Schubfach serializer already uses. No extra memory overhead.

### Serialization

**Schubfach** (Giulietti 2020, ported from yyjson MIT) — implemented in `qj_nc::f64_to_dec()`.  Produces the unique shortest decimal representation in O(1) via 128-bit lookup.  No trailing zeros, no round-trip loss.

### Test coverage

```cpp
// tests/test_serializer.cpp — excerpt
TEST(Roundtrip, AllSpecialDoubles) {
    for (double v : {0.0, -0.0, 1.0, -1.0, DBL_MIN, DBL_MAX,
                     1.0/3.0, M_PI, 1e300, 5e-324}) {
        auto s = qbuem::write(v);
        EXPECT_EQ(qbuem::read<double>(s), v);
    }
}
```

---

## Test suite breakdown

| File | Tests | What it covers |
|:---|---:|:---|
| `test_value_accessors.cpp` | ~200 | DOM `Value` access: `as<T>()`, `try_as<T>()`, `is_*()`, `operator[]`, `at()` |
| `test_stl_exhaustive.cpp` | ~120 | All STL container types via Nexus: vector, map, set, list, deque, optional, variant, tuple, pair |
| `test_complex_stl.cpp` | ~80 | Deeply nested STL combinations (`map<string, vector<optional<int>>>` etc.) |
| `test_compliance.cpp` | **73** | RFC 8259, RFC 6901, RFC 6902 — canonical spec conformance |
| `test_dom_roundtrip.cpp` | ~20 | Parse → mutate → re-serialise equality |
| `test_mutations.cpp` | ~25 | In-place scalar and structural mutations |
| `test_unicode.cpp` | ~15 | UTF-8 validation, `\uXXXX` escape decode, surrogate pairs |
| `test_serializer.cpp` | ~20 | Schubfach floats, yy-itoa integers, edge cases (NaN, Inf, DBL_MAX) |
| `test_errors.cpp` | ~15 | Parse error propagation, type error messages, lifetime errors |
| `repro_bugs.cpp` | ~40 | Regression tests for every previously-reported bug |
| `test_control_char.cpp` | ~10 | Bare control characters in strings (strict reject, lenient accept) |
| `test_utf8_validation.cpp` | ~10 | Malformed UTF-8 sequences |
| `test_trailing_commas.cpp` | ~8 | Trailing comma behaviour in strict vs lenient mode |
| `test_duplicate_keys.cpp` | ~8 | Duplicate key last-write-wins semantics |
| `test_comments.cpp` | ~8 | `//` and `/* */` comment handling in lenient mode |
| `test_relaxed.cpp` | ~8 | Lenient mode acceptance of non-standard extensions |
| `test_bitmap.cpp` | ~6 | SIMD bitmask correctness for structural characters |
| `test_bitmap_offsets.cpp` | ~6 | Bitmask byte-offset alignment edge cases |
| `test_dom_types.cpp` | ~10 | DOM type tag correctness |

---

## Running the full test suite yourself

```bash
git clone https://github.com/qbuem/qbuem-json.git
cd qbuem-json

# Standard build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DQBUEM_JSON_BUILD_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

# Expected output:
# 675/675 Test #675: StringEdge.SpecialKeyNames .......... Passed  0.00 sec
# 100% tests passed, 0 tests failed out of 675
```

The test suite takes under 1 second on any modern machine.

---

## CodeQL static analysis

A [CodeQL workflow](https://github.com/qbuem/qbuem-json/actions/workflows/codeql.yml)
runs the `security-extended` query suite on every push to `main` and on a
weekly schedule.  This catches:

- Buffer overruns, integer overflows, uncontrolled format strings
- Use of unsafe C standard library functions
- Injection-related patterns (not directly applicable to a JSON library, but
  caught proactively)

Results are visible in the
[GitHub Security tab](https://github.com/qbuem/qbuem-json/security/code-scanning).
