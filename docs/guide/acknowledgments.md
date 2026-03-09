# Acknowledgments & References

Beast JSON is built upon the shoulders of giants. Our performance and correctness are the result of integrating decades of research in parsing, string processing, and mathematical scaling.

We would like to express our deepest gratitude to the following individuals and projects whose work made Beast JSON possible.

## 🔬 Mathematical Foundations

### Russ Cox (Google)
- **Fast Unrounded Scaling**: The cornerstone of our bit-accurate number parsing. His research simplified what was historically a complex and error-prone part of systems programming.
- [Floating-Point Printing and Parsing Can Be Simple and Fast (2026)](https://research.swtch.com/fp-all/)

### Daniel Lemire & Michael Eisel
- **Eisel-Lemire Algorithm**: For providing the high-speed 64-bit parsing path that handles 99% of our number processing.
- [fast_float: 28 GB/s floating-point parsing](https://github.com/fastfloat/fast_float)

## 🏎️ SIMD & Architectural Inspiration

### simdjson Project
- **Geoff Langdale & Daniel Lemire**: For the groundbreaking work on two-phase SIMD parsing and structural indexing which heavily influenced our Tape DOM design.
- [simdjson: Parsing gigabytes of JSON per second](https://github.com/simdjson/simdjson)

### RapidJSON
- **Milo Yip**: For creating the "gold standard" of C++ JSON libraries for a decade and providing the modular Handler-based design inspiration.
- [RapidJSON Documentation](https://rapidjson.org/)

## 🛠️ Infrastructure & Tools

- **VitePress**: For the beautiful documentation framework.
- **Mermaid.js**: For the technical diagramming engine.
- **Google Benchmark**: For providing the tools to prove our performance claims.

---

::: tip ❤️ To the C++ Community
To the C++ community: Thank you for pushing the boundaries of what is possible with modern C++.
:::
