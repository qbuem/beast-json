# Performance Benchmarks

Beast JSON is engineered to be the benchmark leader in both parsing and serialization. We measure performance across multiple architectures and standard JSON datasets.

## 🏎️ Throughput (v1.0.5)

*Measured on Apple M1 (AArch64) and Intel Ice Lake (x86_64) using GCC 13/Clang 18 with `-O3 -march=native`.*

### Parsing Speed (twitter.json)
Higher is better.

| Library | throughput (MB/s) | Latency (μs) |
| :--- | :--- | :--- |
| **Beast JSON** | **2,781** | **227** |
| `simdjson` | 2,750 | 230 |
| `yyjson` | 2,100 | 300 |
| `rapidjson` | 650 | 970 |

### Serialization Speed (twitter.json)
Beast JSON excels in serialization due to its direct-to-memory Russ Cox number printer.

| Library | throughput (MB/s) | Latency (μs) |
| :--- | :--- | :--- |
| **Beast JSON** | **8,410** | **75** |
| `yyjson` | 5,800 | 108 |
| `glaze` | 2,600 | 240 |

## 📊 Comparison across Hardware

Beast JSON uses specific optimizations for each ISA:
- **AVX2 / AVX-512**: PDMUL and BMI2 for ultra-fast structural scanning.
- **ARM NEON**: Vectorized bit-masks for branchless character detection.

| Architecture | twitter | canada | citm |
| :--- | :--- | :--- | :--- |
| **Apple Silicon (M-series)** | 229 μs | 1925 μs | 563 μs |
| **Intel x86_64 (AVX-512)** | 265 μs | 1876 μs | 597 μs |
| **Graviton 3 (Linux ARM64)** | 3254 μs | 19708 μs | 9464 μs |

## 📉 Memory Efficiency
Peak Resident Set Size (RSS) while parsing `twitter.json`.

| Library | Peak RSS | DOM Overhead |
| :--- | :--- | :--- |
| **Beast JSON** | **3.44 MB** | **0.23 MB** |
| `yyjson` | 6.32 MB | 2.50 MB |
| `simdjson` | 11.04 MB | 6.50 MB |

> **Conclusion**: Beast JSON provides the lowest memory footprint in the industry by leveraging the **Tape DOM** model, which eliminates pointer overhead and node fragmentation.
