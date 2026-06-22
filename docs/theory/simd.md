# SIMD Acceleration: Bitsliced Structural Analysis

qbuem-json replaces a character-by-character state machine with a **data-parallel byte classification engine**. Rather than branching on each byte, it classifies 64 bytes simultaneously using a single AVX-512 register, producing a sparse bitset of structural positions in a fraction of the time.

---

## The Scalar Baseline — Why It's Slow

A naive JSON scanner must branch on every byte:

```cpp
for (size_t i = 0; i < len; ++i) {
    char c = input[i];
    if      (c == '{') tape_push(OBJ_START);
    else if (c == '}') tape_push(OBJ_END);
    else if (c == '"') handle_string(i);
    // ... 6 more branches
}
```

On a modern superscalar CPU, this produces:
- A branch per byte → branch predictor thrash on real-world JSON
- One byte processed per iteration → unable to exploit instruction-level parallelism
- Maximum throughput: ~1 byte/cycle → ~3 GB/s at 3 GHz

qbuem-json's SIMD path classifies **64 bytes per instruction** on AVX-512 — it replaces 64 byte-at-a-time branches with a handful of branchless vector ops. The end-to-end throughput that buys is quantified in the table at the bottom of this page.

---

## Stage 1 → Stage 2: Interactive Pipeline Walkthrough

Step through the SIMD pipeline interactively — from raw bytes to TapeNodes:

<SimdPipeline />

On Intel Ice Lake and later, `VMOVDQU64` has 1 cycle latency and can be pipelined — the CPU overlaps loading window N+1 while processing window N.

---

## Stage 1a: Parallel Structural Character Detection

Instead of one branch per byte, qbuem-json builds a small set of 64-bit masks with `_mm512_cmpeq_epi8_mask` — one for `"`, one for `\`, the four brackets `{ } [ ]` folded together, the separators `:` `,` — plus a single signed `_mm512_cmpgt_epi8_mask` (`> 0x20`) to find whitespace. Each instruction compares all 64 bytes at once and yields a `__mmask64`. OR-folding the bracket and quote masks gives a **64-bit integer** that marks every candidate structural position in a 64-byte window — a few vector instructions, no per-byte branch.

### What the mask looks like

For the input `{ "name": "Alice" }` (first 20 bytes shown):

<div class="bd-mask-table">
  <div class="bd-mt-row">
    <span class="bd-mt-label">Byte</span>
    <span class="bd-mt-cell bd-mt-cell--idx">0</span><span class="bd-mt-cell bd-mt-cell--idx">1</span><span class="bd-mt-cell bd-mt-cell--idx">2</span><span class="bd-mt-cell bd-mt-cell--idx">3</span><span class="bd-mt-cell bd-mt-cell--idx">4</span><span class="bd-mt-cell bd-mt-cell--idx">5</span><span class="bd-mt-cell bd-mt-cell--idx">6</span><span class="bd-mt-cell bd-mt-cell--idx">7</span><span class="bd-mt-cell bd-mt-cell--idx">8</span><span class="bd-mt-cell bd-mt-cell--idx">9</span><span class="bd-mt-cell bd-mt-cell--idx">10</span><span class="bd-mt-cell bd-mt-cell--idx">11</span><span class="bd-mt-cell bd-mt-cell--idx">12</span><span class="bd-mt-cell bd-mt-cell--idx">13</span><span class="bd-mt-cell bd-mt-cell--idx">14</span><span class="bd-mt-cell bd-mt-cell--idx">15</span><span class="bd-mt-cell bd-mt-cell--idx">16</span><span class="bd-mt-cell bd-mt-cell--idx">17</span><span class="bd-mt-cell bd-mt-cell--idx">18</span><span class="bd-mt-cell bd-mt-cell--idx">19</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">Input</span>
    <span class="bd-mt-cell bd-mt-cell--struct">{</span><span class="bd-mt-cell"> </span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell">n</span><span class="bd-mt-cell">a</span><span class="bd-mt-cell">m</span><span class="bd-mt-cell">e</span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell bd-mt-cell--struct">:</span><span class="bd-mt-cell"> </span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell">A</span><span class="bd-mt-cell">l</span><span class="bd-mt-cell">i</span><span class="bd-mt-cell">c</span><span class="bd-mt-cell">e</span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell"> </span><span class="bd-mt-cell"> </span><span class="bd-mt-cell bd-mt-cell--struct">}</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">Mask</span>
    <span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span>
  </div>
  <div class="bd-mt-annotation">
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,var(--vp-c-brand-1) 15%,transparent);color:var(--vp-c-brand-1);"><strong>■</strong> structural char detected</span>
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,#4caf50 15%,transparent);color:#4caf50;"><strong>1</strong> = set in mask</span>
    <span class="bd-mt-ann-chip" style="color:var(--vp-c-text-3);">0 = cleared</span>
  </div>
</div>

---

## Stage 1b: Quote-Region Masking (Prefix-XOR Carry)

The raw `structural_mask` still includes characters **inside string literals** — e.g., a `:` inside `"key:val"`. qbuem-json uses a **prefix-XOR carry** to suppress them.

The core insight: `in_string[i] = XOR of all unescaped quote bits from index 0 to i`.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="max-width:520px;width:100%;">
      <div class="bd-group__title">Step 1 — Locate backslashes and quotes</div>
      <div class="bd-group__body bd-group__body--row">
        <div class="bd-box bd-box--orange">backslash_mask<br><small>bit=1 at each '\' position</small></div>
        <div class="bd-box bd-box--purple">raw_quote_mask<br><small>bit=1 at each '"' position</small></div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="max-width:520px;width:100%;">
      <div class="bd-group__title">Step 2 — Suppress escaped quotes</div>
      <div class="bd-group__body">
        <div class="bd-box">escape_mask = backslash_mask <strong>shift left 1</strong><br><small>(marks the byte AFTER each backslash)</small></div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--green">real_quote_mask = raw_quote_mask <strong>AND NOT</strong> escape_mask<br><small>(removes escaped quotes like \")</small></div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-group" style="max-width:520px;width:100%;">
      <div class="bd-group__title">Step 3 — Prefix-XOR via a shift-XOR ladder</div>
      <div class="bd-group__body">
        <div class="bd-box bd-box--teal" style="font-family:var(--vp-font-family-mono);font-size:0.74rem;">x ^= x&lt;&lt;1; x ^= x&lt;&lt;2; x ^= x&lt;&lt;4;<br>x ^= x&lt;&lt;8; x ^= x&lt;&lt;16; x ^= x&lt;&lt;32;<br><small>six shifts + XORs = parallel prefix-XOR of the quote mask (<code>simd::prefix_xor</code>)</small></div>
        <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
        <div class="bd-box bd-box--brand">in_string_mask<br><small>bit[i] = XOR(real_quote_mask[0..i]) — 0=outside string, 1=inside</small></div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-box bd-box--green" style="max-width:440px;">
      clean_structural_mask = structural_mask <strong>AND NOT</strong> in_string_mask
    </div>
  </div>
</div>

### Worked example: colon inside a string

Input: `{"key:val":1}` — the `:` at byte 5 is inside a string and must be suppressed.

<div class="bd-mask-table">
  <div class="bd-mt-row">
    <span class="bd-mt-label">Byte</span>
    <span class="bd-mt-cell bd-mt-cell--idx">0</span><span class="bd-mt-cell bd-mt-cell--idx">1</span><span class="bd-mt-cell bd-mt-cell--idx">2</span><span class="bd-mt-cell bd-mt-cell--idx">3</span><span class="bd-mt-cell bd-mt-cell--idx">4</span><span class="bd-mt-cell bd-mt-cell--idx">5</span><span class="bd-mt-cell bd-mt-cell--idx">6</span><span class="bd-mt-cell bd-mt-cell--idx">7</span><span class="bd-mt-cell bd-mt-cell--idx">8</span><span class="bd-mt-cell bd-mt-cell--idx">9</span><span class="bd-mt-cell bd-mt-cell--idx">10</span><span class="bd-mt-cell bd-mt-cell--idx">11</span><span class="bd-mt-cell bd-mt-cell--idx">12</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">Input</span>
    <span class="bd-mt-cell bd-mt-cell--struct">{</span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell bd-mt-cell--in-str">k</span><span class="bd-mt-cell bd-mt-cell--in-str">e</span><span class="bd-mt-cell bd-mt-cell--in-str">y</span><span class="bd-mt-cell bd-mt-cell--false-pos">:</span><span class="bd-mt-cell bd-mt-cell--in-str">v</span><span class="bd-mt-cell bd-mt-cell--in-str">a</span><span class="bd-mt-cell bd-mt-cell--in-str">l</span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell bd-mt-cell--struct">:</span><span class="bd-mt-cell">1</span><span class="bd-mt-cell bd-mt-cell--struct">}</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">quote_mask</span>
    <span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">in_string</span>
    <span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--in-str">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span>
  </div>
  <div class="bd-mt-row bd-mt-row--spacer"></div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">raw_struct</span>
    <span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--false-pos">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">clean_struct</span>
    <span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--suppressed">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span>
  </div>
  <div class="bd-mt-annotation">
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,#e91e63 12%,transparent);color:#e91e63;"><strong>■</strong> inside string</span>
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,#ff5722 22%,transparent);color:#ff5722;"><strong>■</strong> false positive ← suppressed</span>
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,#4caf50 15%,transparent);color:#4caf50;"><strong>1</strong> real structural char</span>
  </div>
</div>

Byte 5 (`:` inside the string): `raw_struct=1` → `in_string=1` → `clean_struct=0`. Correctly suppressed in one bitwise AND NOT operation.

---

## Stage 1c: From Mask to Positions

There is **no byte-compaction instruction** in the hot path — qbuem-json does not use `VCOMPRESSB`. The 64-bit `clean_structural_mask` is turned into a list of offsets directly: a tight loop pulls the index of each set bit with `__builtin_ctzll`, writes the absolute source offset into a flat `positions[]` array, then clears that bit and repeats.

```cpp
uint32_t base = block_start_offset;
while (structural) {
    int bit = __builtin_ctzll(structural);   // index of next structural char
    positions[count++] = base + bit;          // absolute source offset
    structural &= structural - 1;             // clear lowest set bit
}
```

The product of Stage 1 is `Stage1Index.positions[]` — a dense array holding the byte offset of every structural token. Stage 2 walks **that array**, never the full input. (`ctzll` + `&= x-1` is the same bit-scan idiom you'll see again in Stage 2; here it builds the index, there it drives tape emission.)

---

## Stage 2: Tape Generation

Stage 2 (`parse_staged`) walks the `positions[]` array Stage 1 produced — each entry is the source offset of one structural token — and emits one `TapeNode` per token:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--brand" style="max-width:400px;font-size:0.78rem;">
      Stage1Index.positions[]<br><small style="color:var(--vp-c-text-2);">e.g. [0, 1, 6, 7, 13, …]</small>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div></div>
    <div class="bd-split" style="width:100%;max-width:600px;gap:1rem;">
      <div class="bd-group">
        <div class="bd-group__title">Per-token loop</div>
        <div class="bd-group__body">
          <div class="bd-steps">
            <div class="bd-step"><div class="bd-step__num">1</div><div class="bd-step__body"><div class="bd-step__title">pos = positions[i]</div><div class="bd-step__desc">Next structural offset — one array read</div></div></div>
            <div class="bd-step"><div class="bd-step__num">2</div><div class="bd-step__body"><div class="bd-step__title">Load input[pos]</div><div class="bd-step__desc">Read the structural character</div></div></div>
            <div class="bd-step"><div class="bd-step__num">3</div><div class="bd-step__body"><div class="bd-step__title">dispatch via kActionLut</div><div class="bd-step__desc">256-entry action table → emit TapeNode</div></div></div>
            <div class="bd-step"><div class="bd-step__num">4</div><div class="bd-step__body"><div class="bd-step__title">++i</div><div class="bd-step__desc">advance to the next token → back to step 1</div></div></div>
          </div>
        </div>
      </div>
      <div class="bd-group">
        <div class="bd-group__title">TapeNode emitted per case</div>
        <div class="bd-group__body">
          <div class="bd-box bd-box--teal" style="font-size:0.75rem;">{ } [ ]<br><small>→ ObjectStart/End · ArrayStart/End</small></div>
          <div class="bd-box bd-box--purple" style="font-size:0.75rem;">"<br><small>→ StringRaw (offset, length)</small></div>
          <div class="bd-box bd-box--green" style="font-size:0.75rem;">digit / -<br><small>→ Integer / Double / NumberRaw</small></div>
          <div class="bd-box bd-box--orange" style="font-size:0.75rem;">t / f / n<br><small>→ BooleanTrue / BooleanFalse / Null</small></div>
        </div>
      </div>
    </div>
  </div>
</div>

The loop body executes **once per structural token**. In typical JSON, structural characters are 5–15% of the input — Stage 2 touches only that fraction.

---

## ARM NEON Path

On Apple Silicon and ARM64 servers, qbuem-json uses NEON 128-bit registers (16 bytes per load). The algorithm is identical; 4 NEON iterations cover 64 bytes:

<div class="bd-diagram">
  <div class="bd-group" style="max-width:480px;margin:0 auto;">
    <div class="bd-group__title">One NEON iteration — 16 bytes</div>
    <div class="bd-group__body">
      <div class="bd-pipeline">
        <div class="bd-pipe-stage">
          <div class="bd-pipe-stage__label">Load</div>
          <div class="bd-pipe-stage__main">VLD1Q_U8 q0, [buf]</div>
          <div class="bd-pipe-stage__note">16 bytes · 1 cycle</div>
        </div>
        <div class="bd-pipe-arrow">→</div>
        <div class="bd-pipe-stage">
          <div class="bd-pipe-stage__label">Compare</div>
          <div class="bd-pipe-stage__main">vceqq_u8 / vcgtq_u8</div>
          <div class="bd-pipe-stage__note">vs each structural target + whitespace</div>
        </div>
        <div class="bd-pipe-arrow">→</div>
        <div class="bd-pipe-stage">
          <div class="bd-pipe-stage__label">Reduce</div>
          <div class="bd-pipe-stage__main">vorrq_u8 → neon_movemask</div>
          <div class="bd-pipe-stage__note">merge masks, pack to a 16-bit bitmask</div>
        </div>
      </div>
    </div>
  </div>
</div>

NEON has no single-instruction byte-compaction, so qbuem-json reduces each 16-byte compare result to a bitmask with `neon_movemask` (a weight-vector multiply plus a horizontal `vaddv` add), then feeds the same `ctzll`-style bit-scan as the x86 path. The `prefix_xor` shift-ladder is identical across both ISAs.

---

## Throughput Summary

| Architecture | Register width | Bytes/cycle (Stage 1) | Throughput @ 3 GHz |
|:---|:---|:---|:---|
| x86-64 AVX-512 | 512-bit ZMM | ~8 bytes/cycle | ~24 GB/s (register bandwidth) |
| ARM NEON | 128-bit Q | ~4 bytes/cycle | ~12 GB/s |
| Scalar reference | 8-bit GPR | ~1 byte/cycle | ~3 GB/s |

End-to-end parse throughput (up to ~2.9 GB/s on x86_64, ~2.5 on Apple Silicon) is below the Stage-1 ceiling because memory bandwidth and Stage-2 tape generation are the bottleneck for large documents.

---

## Instruction Reference

These are the intrinsics and builtins the parser actually uses (no `PCLMULQDQ` and no `VCOMPRESSB` — the in-string mask is a shift-XOR ladder and extraction is a `ctzll` loop):

| Intrinsic / builtin | ISA | Operation |
|:---|:---|:---|
| `_mm512_loadu_si512` | AVX-512F | Load 64 bytes unaligned into a ZMM register |
| `_mm512_cmpeq_epi8_mask` | AVX-512BW | Compare 64 bytes for equality → `__mmask64` |
| `_mm512_cmpgt_epi8_mask` | AVX-512BW | Signed compare → whitespace mask (`> 0x20`) |
| `simd::prefix_xor` | portable | 6-step shift-XOR ladder → in-string mask |
| `__builtin_ctzll` | BMI1 (`TZCNT`) | Index of the next set structural bit |
| `x &= x - 1` | — | Clear lowest set bit, advance to the next |
| `vld1q_u8` / `vceqq_u8` | NEON | Load / compare 16 bytes |
| `neon_movemask` | NEON | Reduce a 16-byte compare result to a bitmask |
