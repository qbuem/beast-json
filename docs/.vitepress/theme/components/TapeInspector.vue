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
      idx: 0, tag: 'OBJ_START', tagHex: '0x01', payload: 'jump→5', payHex: '0x000000000000005',
      accent: '#0097a7',
      detail: 'Opening brace. Payload stores the index of the matching OBJ_END (5).\nSkipping the whole object costs one integer read: tape[tape[0].jump].',
      access: 'Auto-handled by the parser. Exposed via root.is_object().',
      cost: '0 allocs · O(1) skip',
    },
    {
      idx: 1, tag: 'KEY', tagHex: '0x05', payload: 'ptr+len → "id"', payHex: '0x00[ptr48][len8]',
      accent: '#00838f',
      detail: 'Key string node. Payload is a 48-bit virtual address pointing into the\ninput buffer, plus an 8-bit inline length field.\nZero bytes are copied. No heap involved.',
      access: 'Matched by root["id"] — compares string_view at this ptr+len.',
      cost: '0 allocs · 0 copies',
    },
    {
      idx: 2, tag: 'INT64', tagHex: '0x08', payload: '42 (inline)', payHex: '0x000000000000002A',
      accent: '#4caf50',
      detail: 'Integer stored entirely inline in the 56-bit payload.\nNo heap node, no pointer. Value is extracted by a single bit-shift:\n  int64_t v = static_cast<int64_t>(node.raw << 8) >> 8;',
      access: 'root["id"].as<int>()  →  reads tape[2], extracts bits 55..0.',
      cost: '1 register read · 0 allocs',
    },
    {
      idx: 3, tag: 'KEY', tagHex: '0x05', payload: 'ptr+len → "ok"', payHex: '0x00[ptr48][len8]',
      accent: '#00838f',
      detail: 'Key string node for "ok".\nSame zero-copy pointer-into-input-buffer strategy as tape[1].',
      access: 'Matched by root["ok"].',
      cost: '0 allocs · 0 copies',
    },
    {
      idx: 4, tag: 'BOOL_TRUE', tagHex: '0x0A', payload: '(unused)', payHex: '0x000000000000000',
      accent: '#ff9800',
      detail: 'Boolean true. The type tag alone carries all the information.\nPayload is always zero — no value to store for a boolean.\nBOOL_FALSE would be tag 0x0B.',
      access: 'root["ok"].as<bool>()  →  tag == 0x0A ? true : false.',
      cost: '1 register read · 0 allocs',
    },
    {
      idx: 5, tag: 'OBJ_END', tagHex: '0x02', payload: 'jump→0', payHex: '0x000000000000000',
      accent: '#0097a7',
      detail: 'Closing brace. Payload stores the index of the matching OBJ_START (0).\nIterating object keys starts at tape[0+1] and stops at tape[5].',
      access: 'Auto-handled by iteration logic.',
      cost: '0 allocs',
    },
  ],
  // Sample 1: { "name": "Alice" }
  [
    {
      idx: 0, tag: 'OBJ_START', tagHex: '0x01', payload: 'jump→3', payHex: '0x000000000000003',
      accent: '#0097a7',
      detail: 'Object container. jump=3 means tape[3] is the matching OBJ_END.',
      access: 'root.is_object()  →  true',
      cost: '0 allocs · O(1) skip',
    },
    {
      idx: 1, tag: 'KEY', tagHex: '0x05', payload: 'ptr+len → "name"', payHex: '0x00[ptr48][len8]',
      accent: '#00838f',
      detail: 'Key node pointing into the input buffer at the "name" substring.\nThe 48-bit address is the actual virtual address of the first byte.\nThe 8-bit length field holds 4 (length of "name").',
      access: 'root["name"]  →  walks tape forward, compares this string_view.',
      cost: '0 allocs · 0 copies',
    },
    {
      idx: 2, tag: 'STRING', tagHex: '0x06', payload: 'ptr+len → "Alice"', payHex: '0x00[ptr48][len8]',
      accent: '#9c27b0',
      detail: 'Value string. Like KEY, stores a zero-copy pointer into the input buffer.\nThe 48-bit address points at the "A" of Alice.\nThe 8-bit length holds 5.\nThe caller\'s input buffer must stay alive as long as this view is used.',
      access: 'root["name"].as<string_view>()  →  reads tape[2], returns {ptr, 5}.',
      cost: '1 array read · 0 allocs · 0 copies',
    },
    {
      idx: 3, tag: 'OBJ_END', tagHex: '0x02', payload: 'jump→0', payHex: '0x000000000000000',
      accent: '#0097a7',
      detail: 'Closing brace. Matches tape[0].',
      access: 'Auto-handled.',
      cost: '0 allocs',
    },
  ],
  // Sample 2: { "tags": [1, 2, 3] }
  [
    {
      idx: 0, tag: 'OBJ_START', tagHex: '0x01', payload: 'jump→7', payHex: '0x000000000000007',
      accent: '#0097a7',
      detail: 'Top-level object. Matching OBJ_END is at tape[7].',
      access: 'root.is_object()  →  true',
      cost: '0 allocs',
    },
    {
      idx: 1, tag: 'KEY', tagHex: '0x05', payload: 'ptr+len → "tags"', payHex: '0x00[ptr48][len8]',
      accent: '#00838f',
      detail: 'Key for the array. Zero-copy pointer into input buffer.',
      access: 'root["tags"]  →  matches this KEY node.',
      cost: '0 allocs',
    },
    {
      idx: 2, tag: 'ARR_START', tagHex: '0x03', payload: 'jump→6', payHex: '0x000000000000006',
      accent: '#e91e63',
      detail: 'Array open bracket. jump=6 points to ARR_END.\nTo skip the entire array in O(1):\n  tape[tape[2].jump]  →  jumps directly to tape[6].',
      access: 'root["tags"].is_array()  →  true',
      cost: '0 allocs · O(1) skip',
    },
    {
      idx: 3, tag: 'UINT64', tagHex: '0x07', payload: '1 (inline)', payHex: '0x000000000000001',
      accent: '#4caf50',
      detail: 'Unsigned integer 1. Stored inline in 56-bit payload.\nUINT64 is used when value fits in 56 bits unsigned (< 2^56).',
      access: 'root["tags"][0].as<int>()  →  reads tape[3].',
      cost: '1 array read · 0 allocs',
    },
    {
      idx: 4, tag: 'UINT64', tagHex: '0x07', payload: '2 (inline)', payHex: '0x000000000000002',
      accent: '#4caf50',
      detail: 'Unsigned integer 2. Inline storage, same pattern as tape[3].',
      access: 'root["tags"][1].as<int>()',
      cost: '1 array read · 0 allocs',
    },
    {
      idx: 5, tag: 'UINT64', tagHex: '0x07', payload: '3 (inline)', payHex: '0x000000000000003',
      accent: '#4caf50',
      detail: 'Unsigned integer 3.',
      access: 'root["tags"][2].as<int>()',
      cost: '1 array read · 0 allocs',
    },
    {
      idx: 6, tag: 'ARR_END', tagHex: '0x04', payload: 'jump→2', payHex: '0x000000000000002',
      accent: '#e91e63',
      detail: 'Array close bracket. jump=2 links back to ARR_START.\nThis enables bidirectional O(1) navigation.',
      access: 'Auto-handled by iteration.',
      cost: '0 allocs',
    },
    {
      idx: 7, tag: 'OBJ_END', tagHex: '0x02', payload: 'jump→0', payHex: '0x000000000000000',
      accent: '#0097a7',
      detail: 'Top-level object close.',
      access: 'Auto-handled.',
      cost: '0 allocs',
    },
  ],
  // Sample 3: { "ratio": 3.14 }
  [
    {
      idx: 0, tag: 'OBJ_START', tagHex: '0x01', payload: 'jump→3', payHex: '0x000000000000003',
      accent: '#0097a7',
      detail: 'Object container.',
      access: 'root.is_object()',
      cost: '0 allocs',
    },
    {
      idx: 1, tag: 'KEY', tagHex: '0x05', payload: 'ptr+len → "ratio"', payHex: '0x00[ptr48][len8]',
      accent: '#00838f',
      detail: 'Key node for "ratio". Zero-copy pointer into input.',
      access: 'root["ratio"]  →  matches this key.',
      cost: '0 allocs',
    },
    {
      idx: 2, tag: 'DOUBLE', tagHex: '0x09', payload: '3.14 (bit-cast)', payHex: '0x400091EB851EB852',
      accent: '#ff5722',
      detail: 'Double-precision float stored as a bit-cast uint64_t.\nThe Russ Cox unrounded algorithm converts "3.14" to the closest IEEE 754\ndouble during Stage 2 — no strtod(), no locale dependency.\nRecovery: bit_cast<double>(payload) returns 3.14 exactly.',
      access: 'root["ratio"].as<double>()  →  bit_cast<double>(tape[2] & mask56).',
      cost: '1 array read · 0 allocs',
    },
    {
      idx: 3, tag: 'OBJ_END', tagHex: '0x02', payload: 'jump→0', payHex: '0x000000000000000',
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
  '0x01': '#0097a7', '0x02': '#0097a7',
  '0x03': '#e91e63', '0x04': '#e91e63',
  '0x05': '#00838f', '0x06': '#9c27b0',
  '0x07': '#4caf50', '0x08': '#4caf50',
  '0x09': '#ff5722',
  '0x0A': '#ff9800', '0x0B': '#ff9800',
  '0x0C': '#607d8b',
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
        <!-- 64-bit layout bar -->
        <div class="ti-bits-section">
          <span class="ti-detail-label">64-bit TapeNode layout:</span>
          <div class="ti-bits-bar">
            <div class="ti-bit-tag" :style="{ background: tagColors[selected.tagHex] || '#666' }">
              <span class="ti-bit-range">63–56</span>
              <span class="ti-bit-value">{{ selected.tagHex }}</span>
              <span class="ti-bit-name">type tag</span>
            </div>
            <div class="ti-bit-pay">
              <span class="ti-bit-range">55–0</span>
              <span class="ti-bit-value">{{ selected.payHex }}</span>
              <span class="ti-bit-name">payload (56 bits)</span>
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
