<script setup lang="ts">
import { ref, computed } from 'vue'

// ── Sample inputs ────────────────────────────────────────────────────────────
interface Sample {
  label: string
  json: string
}
const samples: Sample[] = [
  { label: 'Object',  json: '{ "id": 42, "ok": true }' },
  { label: 'String',  json: '{ "name": "Alice" }' },
  { label: 'Array',   json: '{ "tags": [1, 2, 3] }' },
  { label: 'Float',   json: '{ "ratio": 3.14 }' },
]
const sampleIdx = ref(0)

// ── TapeNode definitions per sample ─────────────────────────────────────────
interface TapeNode {
  idx:     number
  tag:     string
  tagHex:  string
  payload: string
  payHex:  string
  accent:  string
  detail:  string   // human-readable breakdown
  access:  string   // how to access in C++
  cost:    string
}

const tapeSets: TapeNode[][] = [
  // Sample 0: { "id": 42, "ok": true }
  [
    {
      idx: 0, tag: 'ObjectStart', tagHex: '0x09', payload: '—', payHex: 'meta=0x09000000 off=0x00000000',
      accent: '#0097a7',
      detail: 'Opening-brace marker. Containers store NO end-index. To skip the whole\nobject, skip_value_ walks forward counting bracket depth until it returns\nto zero — a sequential scan, not a jump.',
      access: 'Auto-handled by the parser. Exposed via root.is_object().',
      cost: '0 allocs · O(N) sequential skip',
    },
    {
      idx: 1, tag: 'StringRaw', tagHex: '0x05', payload: 'off=3 len=2 → "id"', payHex: 'meta=0x05000002 off=0x00000003',
      accent: '#00838f',
      detail: 'Key node = an (offset, length) slice into the source buffer (offset 3,\nlength 2). No pointer, no 48-bit address — just a 32-bit byte offset and\na 16-bit length. Zero bytes copied, no heap.',
      access: 'Matched by root["id"] — compares the string_view at (offset, length).',
      cost: '0 allocs · 0 copies',
    },
    {
      idx: 2, tag: 'Integer', tagHex: '0x03', payload: 'off=8 len=2 → "42"', payHex: 'meta=0x03000002 off=0x00000008',
      accent: '#4caf50',
      detail: 'Integer token "42", stored as an (offset, length) slice — NOT inline.\nThe digits are converted only on demand: .as<int>() runs from_chars over\nsource[8..10]. A field you never read costs nothing.',
      access: 'root["id"].as<int>()  →  from_chars(source+8, 2)  →  42.',
      cost: '0 allocs · parsed on access',
    },
    {
      idx: 3, tag: 'StringRaw', tagHex: '0x05', payload: 'off=13 len=2 → "ok"', payHex: 'meta=0x05000002 off=0x0000000D',
      accent: '#00838f',
      detail: 'Key node for "ok". Same (offset, length) zero-copy slice as tape[1].',
      access: 'Matched by root["ok"].',
      cost: '0 allocs · 0 copies',
    },
    {
      idx: 4, tag: 'BooleanTrue', tagHex: '0x01', payload: '—', payHex: 'meta=0x01000000 off=0x00000012',
      accent: '#ff9800',
      detail: 'Boolean true. The type tag (0x01) carries all the information — there is\nnothing else to store. BooleanFalse is 0x02.',
      access: 'root["ok"].as<bool>()  →  type == BooleanTrue ? true : false.',
      cost: '1 tape read · 0 allocs',
    },
    {
      idx: 5, tag: 'ObjectEnd', tagHex: '0x0A', payload: '—', payHex: 'meta=0x0A000000 off=0x00000017',
      accent: '#0097a7',
      detail: 'Closing-brace marker. Object iteration starts at tape[1] and stops when\nit reaches this ObjectEnd — no back-pointer needed.',
      access: 'Auto-handled by iteration logic.',
      cost: '0 allocs',
    },
  ],
  // Sample 1: { "name": "Alice" }
  [
    {
      idx: 0, tag: 'ObjectStart', tagHex: '0x09', payload: '—', payHex: 'meta=0x09000000 off=0x00000000',
      accent: '#0097a7',
      detail: 'Object container marker. No end-index is stored; skip_value_ walks to\nthe matching ObjectEnd when a subtree is skipped.',
      access: 'root.is_object()  →  true',
      cost: '0 allocs · O(N) sequential skip',
    },
    {
      idx: 1, tag: 'StringRaw', tagHex: '0x05', payload: 'off=3 len=4 → "name"', payHex: 'meta=0x05000004 off=0x00000003',
      accent: '#00838f',
      detail: 'Key node = (offset, length) slice into the input at the "name" substring.\nA 32-bit byte offset plus a 16-bit length — no virtual address, no copy.',
      access: 'root["name"]  →  walks tape forward, compares this string_view.',
      cost: '0 allocs · 0 copies',
    },
    {
      idx: 2, tag: 'StringRaw', tagHex: '0x05', payload: 'off=11 len=5 → "Alice"', payHex: 'meta=0x05000005 off=0x0000000B',
      accent: '#00838f',
      detail: 'Value string — same StringRaw node type as a key, an (offset, length)\nslice pointing at the "A" of Alice. The caller\'s input buffer must stay\nalive as long as this view is used (decoded() copies if you need escapes resolved).',
      access: 'root["name"].as<string_view>()  →  {source+11, 5}.',
      cost: '1 tape read · 0 allocs · 0 copies',
    },
    {
      idx: 3, tag: 'ObjectEnd', tagHex: '0x0A', payload: '—', payHex: 'meta=0x0A000000 off=0x00000012',
      accent: '#0097a7',
      detail: 'Closing-brace marker for the object opened at tape[0].',
      access: 'Auto-handled.',
      cost: '0 allocs',
    },
  ],
  // Sample 2: { "tags": [1, 2, 3] }
  [
    {
      idx: 0, tag: 'ObjectStart', tagHex: '0x09', payload: '—', payHex: 'meta=0x09000000 off=0x00000000',
      accent: '#0097a7',
      detail: 'Top-level object marker. No end-index stored.',
      access: 'root.is_object()  →  true',
      cost: '0 allocs',
    },
    {
      idx: 1, tag: 'StringRaw', tagHex: '0x05', payload: 'off=3 len=4 → "tags"', payHex: 'meta=0x05000004 off=0x00000003',
      accent: '#00838f',
      detail: 'Key for the array — an (offset, length) slice into the input.',
      access: 'root["tags"]  →  matches this StringRaw key node.',
      cost: '0 allocs',
    },
    {
      idx: 2, tag: 'ArrayStart', tagHex: '0x07', payload: '—', payHex: 'meta=0x07000000 off=0x0000000A',
      accent: '#e91e63',
      detail: 'Array-open marker. Like objects, it stores no end-index: skip_value_\nstreams over the elements (tape[3..5]) to skip the whole array.',
      access: 'root["tags"].is_array()  →  true',
      cost: '0 allocs · O(N) sequential skip',
    },
    {
      idx: 3, tag: 'Integer', tagHex: '0x03', payload: 'off=11 len=1 → "1"', payHex: 'meta=0x03000001 off=0x0000000B',
      accent: '#4caf50',
      detail: 'Integer token "1" as an (offset, length) slice. There is no separate\nunsigned type — Integer covers it; the value is parsed on demand.',
      access: 'root["tags"][0].as<int>()  →  from_chars(source+11, 1)  →  1.',
      cost: '0 allocs · parsed on access',
    },
    {
      idx: 4, tag: 'Integer', tagHex: '0x03', payload: 'off=14 len=1 → "2"', payHex: 'meta=0x03000001 off=0x0000000E',
      accent: '#4caf50',
      detail: 'Integer token "2" — same (offset, length) pattern as tape[3].',
      access: 'root["tags"][1].as<int>()',
      cost: '0 allocs · parsed on access',
    },
    {
      idx: 5, tag: 'Integer', tagHex: '0x03', payload: 'off=17 len=1 → "3"', payHex: 'meta=0x03000001 off=0x00000011',
      accent: '#4caf50',
      detail: 'Integer token "3".',
      access: 'root["tags"][2].as<int>()',
      cost: '0 allocs · parsed on access',
    },
    {
      idx: 6, tag: 'ArrayEnd', tagHex: '0x08', payload: '—', payHex: 'meta=0x08000000 off=0x00000012',
      accent: '#e91e63',
      detail: 'Array-close marker for the array opened at tape[2]. No back-pointer.',
      access: 'Auto-handled by iteration.',
      cost: '0 allocs',
    },
    {
      idx: 7, tag: 'ObjectEnd', tagHex: '0x0A', payload: '—', payHex: 'meta=0x0A000000 off=0x00000014',
      accent: '#0097a7',
      detail: 'Top-level object close.',
      access: 'Auto-handled.',
      cost: '0 allocs',
    },
  ],
  // Sample 3: { "ratio": 3.14 }
  [
    {
      idx: 0, tag: 'ObjectStart', tagHex: '0x09', payload: '—', payHex: 'meta=0x09000000 off=0x00000000',
      accent: '#0097a7',
      detail: 'Object container marker.',
      access: 'root.is_object()',
      cost: '0 allocs',
    },
    {
      idx: 1, tag: 'StringRaw', tagHex: '0x05', payload: 'off=3 len=5 → "ratio"', payHex: 'meta=0x05000005 off=0x00000003',
      accent: '#00838f',
      detail: 'Key node for "ratio" — an (offset, length) slice into the input.',
      access: 'root["ratio"]  →  matches this key.',
      cost: '0 allocs',
    },
    {
      idx: 2, tag: 'Double', tagHex: '0x04', payload: 'off=11 len=4 → "3.14"', payHex: 'meta=0x04000004 off=0x0000000B',
      accent: '#ff5722',
      detail: 'Double token "3.14" — stored as an (offset, length) slice, NOT a\nbit-cast value. The digits are converted only on demand: .as<double>()\nruns the Eisel-Lemire fast path (Russ-Cox unrounded scaling for the ~1% of\nhalfway cases) over source[11..15] — no strtod, no locale dependency.',
      access: 'root["ratio"].as<double>()  →  Eisel-Lemire parse of source[11..15].',
      cost: '0 allocs · parsed on access',
    },
    {
      idx: 3, tag: 'ObjectEnd', tagHex: '0x0A', payload: '—', payHex: 'meta=0x0A000000 off=0x00000010',
      accent: '#0097a7',
      detail: 'Object close.',
      access: 'Auto-handled.',
      cost: '0 allocs',
    },
  ],
]

const activeNodes = computed(() => tapeSets[sampleIdx.value])
const selectedIdx = ref<number | null>(null)
const selected    = computed(() =>
  selectedIdx.value !== null ? activeNodes.value[selectedIdx.value] : null
)

function selectNode(idx: number) {
  selectedIdx.value = selectedIdx.value === idx ? null : idx
}

function changeSample(i: number) {
  sampleIdx.value  = i
  selectedIdx.value = null
}

// 64-bit breakdown visual
const tagColors: Record<string, string> = {
  '0x00': '#607d8b',
  '0x01': '#ff9800', '0x02': '#ff9800',
  '0x03': '#4caf50', '0x04': '#ff5722',
  '0x05': '#00838f', '0x06': '#9c27b0',
  '0x07': '#e91e63', '0x08': '#e91e63',
  '0x09': '#0097a7', '0x0A': '#0097a7',
}
</script>

<template>
  <div class="ti-wrap">
    <!-- Sample selector -->
    <div class="ti-samples">
      <button
        v-for="(s, i) in samples"
        :key="i"
        class="ti-sample-btn"
        :class="{ active: sampleIdx === i }"
        @click="changeSample(i)"
      >{{ s.label }}</button>
    </div>

    <!-- JSON input display -->
    <div class="ti-json-row">
      <span class="ti-label">Input:</span>
      <code class="ti-json">{{ samples[sampleIdx].json }}</code>
    </div>

    <!-- Tape grid -->
    <div class="ti-tape-label">
      <span class="ti-label">TapeArena — click any node to inspect</span>
    </div>
    <div class="ti-tape">
      <button
        v-for="n in activeNodes"
        :key="n.idx"
        class="ti-node"
        :class="{ 'ti-node--active': selectedIdx === n.idx }"
        :style="{ '--acc': n.accent }"
        @click="selectNode(n.idx)"
        :aria-pressed="selectedIdx === n.idx"
      >
        <span class="ti-node__idx">tape[{{ n.idx }}]</span>
        <span class="ti-node__tag">{{ n.tag }}</span>
        <span class="ti-node__pay">{{ n.payload }}</span>
      </button>
    </div>

    <!-- Detail panel -->
    <Transition name="ti-slide">
      <div v-if="selected" class="ti-detail" :style="{ '--acc': selected.accent }">
        <!-- 8-byte layout bar: meta (type|flags|length) + offset -->
        <div class="ti-bits-section">
          <span class="ti-detail-label">8-byte TapeNode layout:  { uint32 meta, uint32 offset }</span>
          <div class="ti-bits-bar">
            <div class="ti-bit-tag" :style="{ background: tagColors[selected.tagHex] || '#666' }">
              <span class="ti-bit-range">meta 31–24</span>
              <span class="ti-bit-value">{{ selected.tagHex }}</span>
              <span class="ti-bit-name">type · 8b</span>
            </div>
            <div class="ti-bit-pay">
              <span class="ti-bit-range">meta 23–16 / 15–0 · offset 31–0</span>
              <span class="ti-bit-value">{{ selected.payHex }}</span>
              <span class="ti-bit-name">flags · length · source offset</span>
            </div>
          </div>
        </div>

        <!-- Description -->
        <div class="ti-detail-body">
          <div class="ti-detail-section">
            <span class="ti-detail-label">Storage detail:</span>
            <p class="ti-detail-text">{{ selected.detail }}</p>
          </div>
          <div class="ti-detail-section">
            <span class="ti-detail-label">C++ access:</span>
            <code class="ti-detail-code">{{ selected.access }}</code>
          </div>
          <div class="ti-detail-section">
            <span class="ti-detail-label">Cost:</span>
            <span class="ti-detail-cost">{{ selected.cost }}</span>
          </div>
        </div>
      </div>
    </Transition>

    <p v-if="!selected" class="ti-hint">↑ Click a TapeNode to see its 64-bit encoding and storage detail.</p>
  </div>
</template>

<style scoped>
.ti-wrap {
  border-radius: 12px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  margin: 2rem 0;
  overflow: hidden;
  font-family: var(--vp-font-family-mono);
}

/* ── sample buttons ── */
.ti-samples {
  display: flex;
  border-bottom: 1px solid var(--vp-c-divider);
  flex-wrap: wrap;
}
.ti-sample-btn {
  flex: 1;
  padding: 0.6rem 0.5rem;
  background: none;
  border: none;
  border-bottom: 3px solid transparent;
  cursor: pointer;
  font-size: 0.8rem;
  font-family: inherit;
  color: var(--vp-c-text-2);
  transition: all 0.2s;
  min-width: 70px;
}
.ti-sample-btn:hover { background: var(--vp-c-bg-mute); color: var(--vp-c-text-1); }
.ti-sample-btn.active {
  border-bottom-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
  font-weight: 700;
}

/* ── json row ── */
.ti-json-row {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.75rem 1.25rem 0.25rem;
  flex-wrap: wrap;
}
.ti-label {
  font-size: 0.68rem;
  text-transform: uppercase;
  letter-spacing: 0.07em;
  color: var(--vp-c-text-3);
  flex-shrink: 0;
}
.ti-json {
  font-size: 0.85rem;
  color: var(--vp-c-brand-1);
  background: var(--vp-c-bg-mute);
  padding: 0.25rem 0.65rem;
  border-radius: 4px;
  white-space: nowrap;
  overflow-x: auto;
  max-width: 100%;
}

/* ── tape label ── */
.ti-tape-label {
  padding: 0.5rem 1.25rem 0.25rem;
}

/* ── tape nodes ── */
.ti-tape {
  display: flex;
  flex-wrap: wrap;
  gap: 5px;
  padding: 0.25rem 1rem 0.75rem;
}
.ti-node {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  padding: 0.45rem 0.7rem;
  border: 2px solid var(--acc, var(--vp-c-brand-1));
  border-radius: 6px;
  background: color-mix(in srgb, var(--acc, var(--vp-c-brand-1)) 7%, transparent);
  cursor: pointer;
  font-family: inherit;
  min-width: 88px;
  transition: all 0.18s ease;
}
.ti-node:hover {
  background: color-mix(in srgb, var(--acc, var(--vp-c-brand-1)) 15%, transparent);
  transform: translateY(-2px);
}
.ti-node--active {
  background: color-mix(in srgb, var(--acc, var(--vp-c-brand-1)) 22%, transparent);
  box-shadow: 0 0 12px color-mix(in srgb, var(--acc, var(--vp-c-brand-1)) 50%, transparent);
  transform: translateY(-2px);
}
.ti-node__idx { font-size: 0.6rem; color: var(--vp-c-text-3); }
.ti-node__tag { font-size: 0.7rem; font-weight: 700; color: var(--acc, var(--vp-c-brand-1)); }
.ti-node__pay { font-size: 0.63rem; color: var(--vp-c-text-2); text-align: center; }

/* ── detail panel ── */
.ti-detail {
  margin: 0 1rem 1rem;
  border-radius: 8px;
  border: 1px solid var(--acc);
  background: color-mix(in srgb, var(--acc) 6%, var(--vp-c-bg-mute));
  overflow: hidden;
}

/* 64-bit bar */
.ti-bits-section {
  padding: 0.75rem 1rem 0.5rem;
  border-bottom: 1px solid var(--vp-c-divider);
}
.ti-bits-bar {
  display: flex;
  height: 54px;
  border-radius: 6px;
  overflow: hidden;
  margin-top: 0.4rem;
  font-size: 0.65rem;
  gap: 2px;
}
.ti-bit-tag {
  width: 90px;
  flex-shrink: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 1px;
  color: rgba(255,255,255,0.9);
  border-radius: 4px 0 0 4px;
}
.ti-bit-pay {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 1px;
  background: color-mix(in srgb, var(--acc) 18%, transparent);
  border-radius: 0 4px 4px 0;
  border: 1px solid color-mix(in srgb, var(--acc) 40%, transparent);
  color: var(--vp-c-text-1);
}
.ti-bit-range { font-size: 0.58rem; opacity: 0.75; }
.ti-bit-value { font-size: 0.65rem; font-weight: 700; font-family: monospace; }
.ti-bit-name  { font-size: 0.58rem; opacity: 0.75; }

/* detail body */
.ti-detail-body {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
  padding: 0.85rem 1rem;
}
.ti-detail-section { display: flex; flex-direction: column; gap: 0.2rem; }
.ti-detail-label {
  font-size: 0.65rem;
  text-transform: uppercase;
  letter-spacing: 0.07em;
  color: var(--acc);
  font-weight: 700;
}
.ti-detail-text {
  margin: 0;
  font-size: 0.8rem;
  color: var(--vp-c-text-1);
  line-height: 1.55;
  font-family: var(--vp-font-family-base);
  white-space: pre-line;
}
.ti-detail-code {
  font-size: 0.8rem;
  color: var(--vp-c-brand-1);
  background: var(--vp-c-bg-soft);
  padding: 0.25rem 0.6rem;
  border-radius: 4px;
  width: fit-content;
  display: block;
}
.ti-detail-cost {
  font-size: 0.8rem;
  color: #4caf50;
  font-weight: 600;
}

/* hint */
.ti-hint {
  text-align: center;
  font-size: 0.75rem;
  color: var(--vp-c-text-3);
  padding: 0.5rem;
  margin: 0;
  font-family: var(--vp-font-family-base);
}

/* ── slide transition ── */
.ti-slide-enter-active { transition: all 0.25s ease; }
.ti-slide-enter-from   { opacity: 0; transform: translateY(-8px); }
.ti-slide-leave-active { transition: all 0.15s ease; }
.ti-slide-leave-to     { opacity: 0; transform: translateY(-4px); }

/* ── mobile ── */
@media (max-width: 560px) {
  .ti-node { min-width: 72px; padding: 0.35rem 0.45rem; }
  .ti-node__tag { font-size: 0.6rem; }
  .ti-bits-bar { height: 44px; }
  .ti-bit-tag { width: 70px; }
}
</style>
