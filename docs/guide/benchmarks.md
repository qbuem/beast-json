# Performance Benchmarks

qbuem-json is engineered to be the benchmark leader in both parsing and serialization across all major architectures.

Automatically updated on every push to `main` that touches `include/` or `benchmarks/`.
Runs in parallel on three native GitHub-hosted runners (x86\_64 / Apple Silicon / Linux AArch64).

> **All benchmark data is open.**  The raw JSON results file is committed to the repository at
> [`docs/public/benchmark-results.json`](https://github.com/qbuem/qbuem-json/blob/main/docs/public/benchmark-results.json)
> and the [workflow that produces it](https://github.com/qbuem/qbuem-json/actions/workflows/benchmark.yml)
> is fully public.  You can run the exact same suite yourself — see
> [Reproducing our benchmarks](#reproducing-our-benchmarks) below.

| Runner | Architecture | Compiler |
|:---|:---|:---|
| `ubuntu-latest` | x86_64 | GCC 13, Release |
| `ubuntu-24.04-arm` | Linux aarch64 | GCC 14, Release |
| `macos-latest` | Apple Silicon | Apple Clang, Release |

<BenchmarkCi />

---

## 🔗 Language Bindings Performance

`qbuem-json` provides high-performance bindings for **Python**, **Go**, and **Rust**. The automated benchmark dashboard above includes a dedicated section for these bindings, comparing them against the native JSON libraries in each language.

---

## 🔗 Object Mapping (Nexus Fusion)

Unlike DOM-based parsing, **Nexus Fusion** maps JSON directly to C++ structs. These benchmarks measure the latency of full deserialization into complex STL types.

> [!TIP]
> Use the "Dataset" selector above to switch between Standard DOM (twitter.json, etc.) and Object Mapping (Simple Object, Extreme Harsh STL) results.

---

## ⚙️ Architecture-Specific Optimizations

| Architecture | Technique | Benefit |
|:---|:---|:---|
| **x86_64** | AVX-512 64B/iter Stage 1 scan | Extracts structural tokens in a single pass, ahead of parsing |
| **x86_64** | BMI2 `PEXT`/`PDEP` | Bit-manipulation for separator extraction without branches |
| **Apple Silicon** | NEON 16B vectorized bitmasks | Branchless character detection tuned for 128-byte M-series cache lines |
| **All** | SWAR (SIMD Within A Register) | 8 bytes/cycle string scan using 64-bit GPR — no SIMD required for short strings |
| **All** | KeyLenCache | O(1) key lookup for repeated object schemas (e.g., JSON arrays of same-shape objects) |
| **All** | Schubfach dtoa (R. Giulietti / yyjson port) | Shortest round-trip double→decimal — 2–3× faster than `std::to_chars(double)`, no trailing zeros |
| **All** | yy-itoa (Y. Yuan / yyjson port) | Integer→decimal via 2-digit table + multiply-shift — zero division instructions |
| **All** | Russ Cox algorithm (parsing only) | Shortest round-trip decimal parsing — no `strtod`, no bignum fallback |


---

## Reproducing our benchmarks

The benchmark suite is open source and you can run it on your own hardware in
under 10 minutes.

### Prerequisites

| Requirement | Notes |
|:---|:---|
| Linux x86_64 or macOS Apple Silicon | Windows is not yet supported for benchmarks |
| GCC 13+ or Clang 18+ | Must support C++20 and `-march=native` |
| CMake 3.21+ | For FetchContent and presets support |
| ~500 MB disk | Benchmark datasets (downloaded automatically) |

### Step-by-step

```bash
# 1. Clone
git clone https://github.com/qbuem/qbuem-json.git
cd qbuem-json

# 2. Configure with benchmarks enabled
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DQBUEM_JSON_BUILD_BENCHMARKS=ON \
  -DCMAKE_CXX_FLAGS="-march=native"

# 3. Build (only the benchmark targets, faster)
cmake --build build --target bench_general bench_structs -j$(nproc)

# 4. Download the standard datasets (twitter, canada, citm, gsoc, harsh)
cmake --build build --target download_bench_data

# 5. Run and emit the results JSON
./build/benchmarks/bench_general  --output results_dom.json
./build/benchmarks/bench_structs  --output results_structs.json
```

The output files use the same schema as `docs/public/benchmark-results.json`,
so you can drop them into the VitePress site for a local comparison.

### Benchmark methodology

| Parameter | Value |
|:---|:---|
| Measurement | Wall-clock time, `std::chrono::steady_clock`, nanosecond resolution |
| Warmup | 5 dry runs before timing |
| Iterations | 100 timed iterations per dataset/library pair |
| Reported value | **Median** of 100 runs (robust against transient OS jitter) |
| CPU affinity | `taskset -c 0` pins to a single core; turbo boost left as-is |
| Allocator | System allocator (`malloc`); no jemalloc or tcmalloc |
| Compiler flags | `-O3 -march=native -DNDEBUG` (same for all libraries) |
| Memory | Allocation measured with a `std::pmr::monotonic_buffer_resource` wrapper |

### Competing library versions tested

| Library | Version | Source |
|:---|:---|:---|
| simdjson | 3.11.x | FetchContent from GitHub |
| yyjson | 0.10.x | FetchContent from GitHub |
| RapidJSON | 1.1.0 + HEAD | FetchContent from GitHub |
| Glaze DOM | 4.x | FetchContent from GitHub |
| nlohmann/json | 3.11.x | FetchContent from GitHub |

All libraries are compiled from source with the same `-O3 -march=native` flags.
No library-specific tuning flags are applied to any competitor.

### Frequently asked questions

**Why is simdjson sometimes faster on twitter.json parse?**

simdjson uses a two-pass architecture that defers validation to a lazy step,
allowing Stage 1 to run without any structural bookkeeping.  qbuem-json's DOM
engine produces a fully indexed tape in one pass, which pays a small overhead
for the generality of O(1) random access.  On streaming/iterator workloads that
don't need random access, simdjson's deferred model wins on parse throughput.
qbuem-json leads on total parse+access+serialise latency for typical use cases.

**Why is the harsh.json serialize time higher for some libraries?**

`harsh.json` is a synthetically generated worst-case dataset with extremely deep
nesting, long strings with many escape sequences, and a high proportion of
floating-point values.  Serialiser throughput on this dataset is limited by
`double`→decimal conversion speed; libraries using `snprintf` or `std::to_chars`
fall significantly behind Schubfach.

**Can I add my own library to the comparison?**

Yes.  The benchmark harness in `benchmarks/` uses a plugin interface.  Open a
pull request with an adapter in `benchmarks/adapters/` following the existing
pattern.
