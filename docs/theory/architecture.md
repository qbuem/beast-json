# Architecture: The Journey of a JSON Byte

> You call `beast::parse(doc, json_text)`.
> 250 nanoseconds later, you hold a queryable document.
> **This page shows exactly what happens in between.**

Every design decision in Beast JSON traces back to one question:
*"Where does time actually go in a conventional JSON parser?"*

The answer determines everything.

---

## Where Time Goes in a Conventional Parser

A typical parser like `nlohmann/json` processes this:

```json
{ "id": 101, "name": "Alice", "active": true }
```

...and produces a result that looks like this in memory:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:540px;">
      <div class="bd-group__title">nlohmann/json memory layout — 6 elements, 6 heap allocations</div>
      <div class="bd-group__body" style="font-family:var(--vp-font-family-mono);font-size:0.78rem;line-height:1.8;">
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:0.3rem 1.5rem;padding:0.25rem 0;">
          <div class="bd-box bd-box--orange" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x5a40 Object node<br><small style="color:var(--vp-c-text-3);">→ ptr to children vec at 0x7f23</small></div>
          <div class="bd-box bd-box--orange" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x7f23 children vector<br><small style="color:var(--vp-c-text-3);">→ heap-allocated</small></div>
          <div class="bd-box" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x3c11 "id"  → 101<br><small style="color:var(--vp-c-text-3);">copied string key</small></div>
          <div class="bd-box" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x9d04 "name" → "Alice"<br><small style="color:var(--vp-c-text-3);">copied string key + value</small></div>
          <div class="bd-box" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x12ef IntNode(101)<br><small style="color:var(--vp-c-text-3);">separate heap node</small></div>
          <div class="bd-box" style="padding:0.3rem 0.5rem;font-size:0.75rem;">0x2a08 "active" → true<br><small style="color:var(--vp-c-text-3);">copied string key</small></div>
        </div>
        <div class="bd-callout" style="margin-top:0.5rem;font-size:0.75rem;font-family:var(--vp-font-family-base);">
          6 nodes scattered across 6 different cache lines → every traversal is a <strong>cache miss chain</strong>
        </div>
      </div>
    </div>
  </div>
</div>

Three problems compound each other:

| Problem | What happens | Cost |
|:---|:---|:---|
| **Heap scatter** | One `malloc` per node → random addresses | Every access = pointer chase = cache miss |
| **Eager string copy** | Every string allocated and copied at parse time | You pay for strings you never read |
| **No skip mechanism** | To skip a nested array of 5,000 elements, visit all 5,000 | O(N) per subtree skip |

Beast JSON solves all three — **with the same mechanism**.

---

## Step 1 — SIMD Structural Scan

The first thing Beast JSON does with your input is *not* parse it.
It **classifies all 64 bytes at once** using a single AVX-512 instruction,
producing a 64-bit bitmask that marks every structural character
(`{`, `}`, `[`, `]`, `"`, `:`, `,`) in the current window.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:580px;">
      <div class="bd-group__title">Stage 1 — 64 bytes in, 64-bit mask out (one AVX-512 pass)</div>
      <div class="bd-group__body">
        <div class="bd-pipeline">
          <div class="bd-pipe-stage">
            <div class="bd-pipe-stage__label">Load</div>
            <div class="bd-pipe-stage__main">VMOVDQU64</div>
            <div class="bd-pipe-stage__note">64 raw bytes → ZMM0</div>
          </div>
          <div class="bd-pipe-arrow">→</div>
          <div class="bd-pipe-stage">
            <div class="bd-pipe-stage__label">Classify</div>
            <div class="bd-pipe-stage__main">VPCMPEQB ×7</div>
            <div class="bd-pipe-stage__note">one mask per structural char</div>
          </div>
          <div class="bd-pipe-arrow">→</div>
          <div class="bd-pipe-stage">
            <div class="bd-pipe-stage__label">Merge</div>
            <div class="bd-pipe-stage__main">KORQ</div>
            <div class="bd-pipe-stage__note">OR all 7 masks → one 64-bit bitset</div>
          </div>
          <div class="bd-pipe-arrow">→</div>
          <div class="bd-pipe-stage">
            <div class="bd-pipe-stage__label">Filter</div>
            <div class="bd-pipe-stage__main">PCLMULQDQ</div>
            <div class="bd-pipe-stage__note">suppress chars inside strings</div>
          </div>
        </div>
        <div style="margin-top:0.75rem;">
          <div class="bd-callout" style="font-size:0.78rem;">
            On ARM (Apple Silicon, aarch64): 4 × NEON 16-byte loads replace one AVX-512 load.
            Algorithm is identical — same masks, same output.
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

The output is a 64-bit integer — one bit per input byte.
A `1` means "structural character here"; a `0` means "data, skip it".

For the input `{ "id": 101, "active": true }`:

<div class="bd-mask-table">
  <div class="bd-mt-row">
    <span class="bd-mt-label">Byte</span>
    <span class="bd-mt-cell bd-mt-cell--idx">0</span><span class="bd-mt-cell bd-mt-cell--idx">1</span><span class="bd-mt-cell bd-mt-cell--idx">2</span><span class="bd-mt-cell bd-mt-cell--idx">3</span><span class="bd-mt-cell bd-mt-cell--idx">4</span><span class="bd-mt-cell bd-mt-cell--idx">5</span><span class="bd-mt-cell bd-mt-cell--idx">6</span><span class="bd-mt-cell bd-mt-cell--idx">7</span><span class="bd-mt-cell bd-mt-cell--idx">8</span><span class="bd-mt-cell bd-mt-cell--idx">9</span><span class="bd-mt-cell bd-mt-cell--idx">10</span><span class="bd-mt-cell bd-mt-cell--idx">11</span><span class="bd-mt-cell bd-mt-cell--idx">12</span><span class="bd-mt-cell bd-mt-cell--idx">13</span><span class="bd-mt-cell bd-mt-cell--idx">14</span><span class="bd-mt-cell bd-mt-cell--idx">15</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">Input</span>
    <span class="bd-mt-cell bd-mt-cell--struct">{</span><span class="bd-mt-cell"> </span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell">i</span><span class="bd-mt-cell">d</span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell bd-mt-cell--struct">:</span><span class="bd-mt-cell"> </span><span class="bd-mt-cell">1</span><span class="bd-mt-cell">0</span><span class="bd-mt-cell">1</span><span class="bd-mt-cell bd-mt-cell--struct">,</span><span class="bd-mt-cell"> </span><span class="bd-mt-cell bd-mt-cell--struct">"</span><span class="bd-mt-cell">a</span><span class="bd-mt-cell">c</span>
  </div>
  <div class="bd-mt-row">
    <span class="bd-mt-label">Mask</span>
    <span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--one">1</span><span class="bd-mt-cell bd-mt-cell--zero">0</span><span class="bd-mt-cell bd-mt-cell--zero">0</span>
  </div>
  <div class="bd-mt-annotation">
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,var(--vp-c-brand-1) 15%,transparent);color:var(--vp-c-brand-1);"><strong>■</strong> structural char</span>
    <span class="bd-mt-ann-chip" style="background:color-mix(in srgb,#4caf50 15%,transparent);color:#4caf50;"><strong>1</strong> = in mask</span>
    <span class="bd-mt-ann-chip" style="color:var(--vp-c-text-3);">0 = data byte, ignored in Stage 2</span>
  </div>
</div>

**Why this matters:** Stage 2 — which actually builds the document — only visits
bytes where the mask is `1`. Typical JSON has 5–15% structural characters.
Stage 2 processes 5–15% of the input, not 100%.

> Deep dive: [SIMD Acceleration →](/theory/simd)

---

## Step 2 — Tape Generation

Stage 2 iterates the bitset using `TZCNT` (count trailing zeros — 1 cycle) to
find each structural position, reads the character, and writes one 8-byte
`TapeNode` per JSON element into a contiguous array called the **tape**.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-box bd-box--brand" style="max-width:400px;">Structural bitset: <code>0b…1_0000_1011_0110_1101</code></div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">Stage 2: TZCNT loop — one node written per set bit</div></div>
    <div class="bd-group" style="width:100%;max-width:580px;">
      <div class="bd-group__title">Result: tape[] — <strong>one contiguous malloc, 8 bytes per element</strong></div>
      <div class="bd-group__body">
        <div class="bd-tape-strip">
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[0]</span><span class="bd-tape-cell__tag">OBJ_START</span><span class="bd-tape-cell__val">jump→5</span></div>
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[1]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">&amp;buf[2] "id"</span></div>
          <div class="bd-tape-cell bd-tape-cell--int"><span class="bd-tape-cell__idx">tape[2]</span><span class="bd-tape-cell__tag">INT64</span><span class="bd-tape-cell__val">101</span></div>
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[3]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">&amp;buf[13] "active"</span></div>
          <div class="bd-tape-cell bd-tape-cell--bool"><span class="bd-tape-cell__idx">tape[4]</span><span class="bd-tape-cell__tag">BOOL_TRUE</span><span class="bd-tape-cell__val">—</span></div>
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[5]</span><span class="bd-tape-cell__tag">OBJ_END</span><span class="bd-tape-cell__val">jump→0</span></div>
        </div>
      </div>
    </div>
  </div>
</div>

Notice what the tape is **not**:
- Not a tree. No child pointers. No parent pointers.
- Not scattered. All 6 elements sit in 48 consecutive bytes — **one cache line + 16 bytes**.
- Not heap-allocated strings. Key nodes store a raw pointer into the original input buffer.

This is what eliminates Problems 1 and 2 in a single structure.

---

## Step 3 — Inside the TapeNode

Every JSON element — object, array, string, integer, float, bool, null — fits in
exactly **8 bytes**:

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-bits">
      <div class="bd-bit-seg" style="width:90px;flex-shrink:0;background:color-mix(in srgb,var(--vp-c-brand-1) 22%,transparent);border-radius:4px 0 0 4px;">
        <span class="bd-bit-seg__range">bits 63–56</span>
        <span class="bd-bit-seg__val">Type Tag</span>
        <span class="bd-bit-seg__name">8 bits</span>
      </div>
      <div class="bd-bit-seg" style="flex:1;background:color-mix(in srgb,var(--vp-c-brand-1) 9%,transparent);border:1px solid var(--vp-c-divider);border-radius:0 4px 4px 0;">
        <span class="bd-bit-seg__range">bits 55–0</span>
        <span class="bd-bit-seg__val">Payload</span>
        <span class="bd-bit-seg__name">56 bits — meaning depends on type</span>
      </div>
    </div>
    <div class="bd-row" style="gap:0.75rem;margin-top:0.75rem;flex-wrap:wrap;">
      <div class="bd-group" style="flex:1;min-width:160px;margin:0;">
        <div class="bd-group__title" style="font-size:0.68rem;">Containers</div>
        <div class="bd-group__body" style="padding:0.35rem;">
          <div class="bd-box bd-box--teal" style="font-size:0.72rem;padding:0.25rem 0.4rem;"><code>OBJ_START / OBJ_END</code><br><code>ARR_START / ARR_END</code><br><small>payload = jump index</small></div>
        </div>
      </div>
      <div class="bd-group" style="flex:1;min-width:160px;margin:0;">
        <div class="bd-group__title" style="font-size:0.68rem;">Strings</div>
        <div class="bd-group__body" style="padding:0.35rem;">
          <div class="bd-box bd-box--purple" style="font-size:0.72rem;padding:0.25rem 0.4rem;"><code>KEY / STRING</code><br><small>payload = 48-bit ptr<br>+ 8-bit length hint</small></div>
        </div>
      </div>
      <div class="bd-group" style="flex:1;min-width:160px;margin:0;">
        <div class="bd-group__title" style="font-size:0.68rem;">Numbers</div>
        <div class="bd-group__body" style="padding:0.35rem;">
          <div class="bd-box bd-box--green" style="font-size:0.72rem;padding:0.25rem 0.4rem;"><code>UINT64 / INT64</code><br><small>payload = value inline</small><br><code>DOUBLE</code><br><small>payload = bit-cast</small></div>
        </div>
      </div>
      <div class="bd-group" style="flex:1;min-width:120px;margin:0;">
        <div class="bd-group__title" style="font-size:0.68rem;">Scalars</div>
        <div class="bd-group__body" style="padding:0.35rem;">
          <div class="bd-box bd-box--orange" style="font-size:0.72rem;padding:0.25rem 0.4rem;"><code>BOOL_TRUE</code><br><code>BOOL_FALSE</code><br><code>NULL_VAL</code><br><small>payload unused</small></div>
        </div>
      </div>
    </div>
    <div class="bd-callout" style="font-size:0.8rem;margin-top:0.5rem;">
      8 bytes × 8 nodes = 64 bytes = <strong>one CPU cache line holds 8 TapeNodes</strong>.
      Sequential scan never misses cache.
    </div>
  </div>
</div>

The 56-bit payload is large enough for a 48-bit virtual address (the limit on x86-64
and ARM64) plus an 8-bit length — enough to encode a `string_view` with no heap involvement.

---

## Step 4 — Strings Are Never Copied

When Stage 2 encounters a string or key, it writes only a pointer.
The actual bytes stay exactly where they were — in your input buffer.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:500px;">
      <div class="bd-group__title">Input buffer — caller-owned, untouched</div>
      <div class="bd-group__body">
        <div class="bd-box bd-box--blue" style="font-family:monospace;letter-spacing:0.04em;">
          { &nbsp;"id":&nbsp;101,&nbsp;"active":&nbsp;true&nbsp;}
          <div style="display:flex;gap:0;margin-top:4px;font-size:0.7rem;color:var(--vp-c-text-3);">
            <span style="margin-left:2px;">0</span>
            <span style="margin-left:14px;">2</span>
            <span style="margin-left:38px;">13</span>
          </div>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">Stage 2 writes pointer, not data</div></div>
    <div class="bd-group" style="width:100%;max-width:500px;">
      <div class="bd-group__title">Tape</div>
      <div class="bd-group__body">
        <div class="bd-tape-strip" style="flex-direction:column;align-items:flex-start;gap:4px;">
          <div class="bd-tape-cell bd-tape-cell--key" style="width:100%;max-width:380px;"><span class="bd-tape-cell__idx">tape[1] KEY</span><span class="bd-tape-cell__val">ptr=&amp;buf[2], len=2  →  "id"</span></div>
          <div class="bd-tape-cell bd-tape-cell--key" style="width:100%;max-width:380px;"><span class="bd-tape-cell__idx">tape[3] KEY</span><span class="bd-tape-cell__val">ptr=&amp;buf[13], len=6  →  "active"</span></div>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">.as&lt;string_view&gt;() reads one TapeNode → returns view into buf</div></div>
    <div class="bd-callout bd-callout--green" style="max-width:500px;font-size:0.82rem;">
      <strong>Zero bytes copied</strong> — at parse time, at navigation time, or at extraction time.<br>
      The string lives in your buffer. The tape just remembers where.
    </div>
  </div>
</div>

**Lifetime contract:** the `string_view` is valid as long as both the `Document`
and the input buffer are alive. If you need the string to outlive the input,
call `.as<std::string>()` — that performs exactly one copy, on demand.

---

## Step 5 — O(1) Subtree Skipping

When Stage 2 writes an `OBJ_START` node, it stores the tape index of the
matching `OBJ_END` in its payload. This is the **jump index** — the mechanism
that eliminates Problem 3.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;">
      <div class="bd-group__title">Tape for: <code>{ "meta": { ... 500 fields ... }, "id": 42 }</code></div>
      <div class="bd-group__body">
        <div class="bd-tape-strip">
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[0]</span><span class="bd-tape-cell__tag">OBJ_START</span><span class="bd-tape-cell__val">jump→507</span></div>
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[1]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">"meta"</span></div>
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[2]</span><span class="bd-tape-cell__tag">OBJ_START</span><span class="bd-tape-cell__val">jump→503</span></div>
          <div class="bd-tape-cell" style="min-width:90px;opacity:0.4;"><span class="bd-tape-cell__idx">tape[3…502]</span><span class="bd-tape-cell__val">500 fields</span></div>
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[503]</span><span class="bd-tape-cell__tag">OBJ_END</span><span class="bd-tape-cell__val">jump→2</span></div>
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">tape[504]</span><span class="bd-tape-cell__tag">KEY</span><span class="bd-tape-cell__val">"id"</span></div>
          <div class="bd-tape-cell bd-tape-cell--int"><span class="bd-tape-cell__idx">tape[505]</span><span class="bd-tape-cell__tag">INT64</span><span class="bd-tape-cell__val">42</span></div>
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">tape[507]</span><span class="bd-tape-cell__tag">OBJ_END</span><span class="bd-tape-cell__val">jump→0</span></div>
        </div>
        <div class="bd-row" style="gap:1rem;margin-top:0.5rem;">
          <div class="bd-callout bd-callout--green" style="flex:1;margin:0;font-size:0.78rem;">
            <strong>Skip "meta" entirely:</strong><br>
            <code>next = tape[tape[2].jump + 1]</code><br>
            = <code>tape[504]</code> = KEY "id"<br>
            Cost: <strong>1 array read</strong> regardless of depth
          </div>
          <div class="bd-callout" style="flex:1;margin:0;font-size:0.78rem;">
            <strong>nlohmann/json equivalent:</strong><br>
            Recurse into 500-field object,<br>
            follow 500 child pointers,<br>
            Cost: <strong>O(500) cache misses</strong>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

This is why Beast JSON's "100 levels deep, 100 elements wide" benchmark stays fast
while other parsers collapse: every level is a single array read, not a recursive descent.

---

## Step 6 — The Value Handle (Lazy Extraction)

After parsing, you don't get a fully decoded document.
You get a `Value` — a 16-byte token that says "I know where in the tape this is":

```cpp
struct Value {          // 16 bytes total
    DocumentView* doc_; //  8 bytes — which document owns this tape
    uint32_t      idx_; //  4 bytes — which tape slot I point at
    uint32_t      pad_; //  4 bytes — padding
};
```

Navigation (`root["user"]["name"]`) walks the tape following key matches and
jump indices. It produces a new `Value` with a different `idx_` — but extracts nothing.

<div class="bd-diagram">
  <div class="bd-col">
    <div class="bd-group" style="width:100%;max-width:580px;">
      <div class="bd-group__title">The three cost tiers of Beast JSON</div>
      <div class="bd-group__body">
        <div class="bd-steps">
          <div class="bd-step">
            <div class="bd-step__num" style="background:color-mix(in srgb,#e53935 20%,transparent);color:#e53935;">1×</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Parse — paid once per document</div>
              <div class="bd-step__desc">
                SIMD scan + tape write. Proportional to input size.
                On a 100 KB document: ~40 μs.
                <strong>Never paid again, even if you query 1,000 times.</strong>
              </div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num" style="background:color-mix(in srgb,#43a047 20%,transparent);color:#43a047;">0×</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Navigate — free</div>
              <div class="bd-step__desc">
                <code>root["user"]["profile"]["city"]</code> walks tape indices.
                No allocation. No heap access. No string copies.
                Cost is proportional to <em>depth</em>, not document size.
              </div>
            </div>
          </div>
          <div class="bd-step">
            <div class="bd-step__num" style="background:color-mix(in srgb,#1e88e5 20%,transparent);color:#1e88e5;">∂</div>
            <div class="bd-step__body">
              <div class="bd-step__title">Extract — paid only for what you read</div>
              <div class="bd-step__desc">
                <code>.as&lt;string_view&gt;()</code> — one tape read, zero copy.<br>
                <code>.as&lt;int64_t&gt;()</code> — one tape read, no parsing.<br>
                <code>.as&lt;std::string&gt;()</code> — one tape read + one string copy.<br>
                Strings you never call <code>.as()</code> on: <strong>cost = 0</strong>.
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

<LazyLifecycle />

This is the "lazy" in Lazy Tape DOM. The tape holds the *shape* of the document.
The actual data surfaces only when you ask for it.

---

## Step 7 — Memory: One Allocation, Reused Forever

The complete document state lives in a single `DocumentView`:

<TapeFlowDiagram />

| Component | What it is | Memory strategy |
|:---|:---|:---|
| `TapeArena` | The `tape[]` array | Single `malloc` — reused on next parse (reset cursor, keep capacity) |
| `Stage1Index` | SIMD structural position list | Single `malloc` — reused on next parse |
| Input buffer | Your JSON bytes | Never copied — `string_view` pointers in |
| `mutations_` | In-place edit overlay | Separate `std::unordered_map` — only allocated when you call `value.set(...)` |

On a second parse into the same `Document`, **no `malloc` is called** as long as
the new document fits the existing tape capacity. This is why `parse_reuse()`
exists — the first parse pays, every subsequent parse is allocation-free.

---

## The Full Journey

Putting all six steps together for `{"id": 101, "active": true}`:

<div class="bd-diagram">
  <div class="bd-col">

    <div class="bd-group" style="width:100%;max-width:600px;">
      <div class="bd-group__title">① Input — 32 bytes, caller-owned</div>
      <div class="bd-group__body">
        <div class="bd-box bd-box--brand" style="font-family:monospace;">{ "id": 101, "active": true }</div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">Stage 1: AVX-512 / NEON — classify 64 bytes → 64-bit structural mask</div></div>

    <div class="bd-group" style="width:100%;max-width:600px;">
      <div class="bd-group__title">② Structural Bitset</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-family:monospace;font-size:0.8rem;">mask = 0b…1010110_10100101  (8 structural chars set)</div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">Stage 2: TZCNT loop — one TapeNode written per set bit</div></div>

    <div class="bd-group" style="width:100%;max-width:600px;">
      <div class="bd-group__title">③ Tape — 48 contiguous bytes, single malloc</div>
      <div class="bd-group__body">
        <div class="bd-tape-strip">
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">OBJ_START</span><span class="bd-tape-cell__val">→5</span></div>
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">KEY</span><span class="bd-tape-cell__val">"id"</span></div>
          <div class="bd-tape-cell bd-tape-cell--int"><span class="bd-tape-cell__idx">INT64</span><span class="bd-tape-cell__val">101</span></div>
          <div class="bd-tape-cell bd-tape-cell--key"><span class="bd-tape-cell__idx">KEY</span><span class="bd-tape-cell__val">"active"</span></div>
          <div class="bd-tape-cell bd-tape-cell--bool"><span class="bd-tape-cell__idx">BOOL_TRUE</span><span class="bd-tape-cell__val">—</span></div>
          <div class="bd-tape-cell bd-tape-cell--obj"><span class="bd-tape-cell__idx">OBJ_END</span><span class="bd-tape-cell__val">→0</span></div>
        </div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">beast::parse() returns</div></div>

    <div class="bd-group" style="width:100%;max-width:600px;">
      <div class="bd-group__title">④ Value handle — 16 bytes</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-size:0.82rem;font-family:monospace;">{ doc=0x…, idx=0 }  ← "I point at tape[0]"</div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">root["active"] — walk tape matching keys (no allocation)</div></div>

    <div class="bd-group" style="width:100%;max-width:600px;">
      <div class="bd-group__title">⑤ Navigate — new Value, nothing extracted</div>
      <div class="bd-group__body">
        <div class="bd-box" style="font-size:0.82rem;font-family:monospace;">{ doc=0x…, idx=4 }  ← "I point at tape[4] (BOOL_TRUE)"</div>
      </div>
    </div>
    <div class="bd-arrow"><div class="bd-arrow__icon">↓</div><div class="bd-arrow__label">.as&lt;bool&gt;() — one tape read</div></div>

    <div class="bd-box bd-box--brand" style="max-width:200px;font-family:monospace;">true</div>

  </div>
</div>

Every `{ doc, idx }` pair in the chain above costs a handful of nanoseconds.
The 101 nanoseconds you never pay: heap allocation, string copy, recursive descent.

---

## How Each Decision Solves a Problem

| Design choice | Which problem it solves | How |
|:---|:---|:---|
| Single `TapeArena` malloc | **Heap scatter** | Sequential layout → prefetcher works; 8 nodes fit one cache line |
| 8-byte `TapeNode` (fixed size) | **Heap scatter** | No per-node pointer; random access = direct multiply |
| Jump index in container nodes | **No skip mechanism** | Skipping 500-field object = one array read (O(1)) |
| `string_view` pointer in KEY/STRING | **Eager string copy** | Zero bytes copied unless `.as<std::string>()` called |
| `Value` = `{doc*, idx}` handle | **Eager string copy** | Navigation has zero extraction cost |
| SIMD Stage 1 structural scan | **Heap scatter** | Structural positions found at memory-bandwidth speed |
| `TapeArena` reuse across parses | **Heap scatter** | Hot-loop parsing: zero `malloc` after warmup |
| **Nexus Fusion** | Latency-critical DTOs | Skips the tape entirely for direct stream-to-struct mapping |

<div class="bd-callout bd-callout--green" style="margin-top:1rem;">
  <strong>You pay the cost of parsing exactly once.</strong><br>
  You pay the cost of navigation <em>never</em>.<br>
  You pay the cost of extraction <em>only for the fields you actually read</em>.
</div>

---

## Head-to-Head: Beast DOM vs Tree DOM vs simdjson

<TreeVsTape />

| | Beast DOM | nlohmann/json | simdjson |
|:---|:---|:---|:---|
| **Memory layout** | Contiguous tape | Scattered heap | Tape (read-only) |
| **Allocs per parse** | **1** | O(N elements) | 2 (tape + strings) |
| **String storage** | Zero-copy `string_view` | Heap `std::string` | Zero-copy `string_view` |
| **Object/array skip** | **O(1) jump index** | O(N) recursion | O(1) jump index |
| **Extraction model** | Lazy — on demand | Eager — at parse | Lazy — on demand |
| **Mutation support** | ✅ overlay map | ✅ in-place | ❌ read-only |
| **Serialize support** | ✅ | ✅ | ❌ |
| **Cache misses/element** | ~0 (sequential) | 1–3 (pointer chase) | ~0 (sequential) |
| **Peak RSS (twitter.json)** | **3.4 MB** | 27.4 MB | 11.0 MB |

---

## Want to Go Deeper?

The architecture above covers the DOM path — `beast::parse()`.
For latency-critical use cases where even the tape is too much overhead:

→ **[Nexus Fusion: Zero-Tape](/theory/nexus-fusion)** — skip the tape entirely,
map JSON directly to your C++ struct in one pass.

For the SIMD mechanics in detail:

→ **[SIMD Acceleration](/theory/simd)** — how AVX-512 classifies 64 bytes in 8 cycles.

For how numbers are printed after the fastest round-trip:

→ **[Numeric Serialization](/theory/numeric-serialization)** — Schubfach dtoa + yy-itoa.
