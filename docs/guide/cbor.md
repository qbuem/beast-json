# CBOR (Binary Serialization)

**One field list, two wire formats.** The same `QBUEM_JSON_FIELDS` registration that drives JSON also drives a [CBOR (RFC 8949)](https://www.rfc-editor.org/rfc/rfc8949.html) binary codec — compact, self-describing bytes that decode in **any language** (e.g. JavaScript [`cbor-x`](https://github.com/kriszyp/cbor-x)) with no shared schema. Register a struct once; serialize it to both text and binary.

```cpp
#include <qbuem_json/qbuem_json.hpp>

struct Child {
    int64_t     id;
    std::string name;
    std::string birth_date;
};
QBUEM_JSON_FIELDS(Child, id, name, birth_date)   // outside the struct, namespace scope

Child c{ 42, "Mina", "2018-04-01" };

std::string bytes = qbuem::cbor::encode(c);       // → compact CBOR bytes
Child       back  = qbuem::cbor::decode<Child>(bytes);   // ← back to the struct
```

That's the whole story: `qbuem::cbor::encode` / `qbuem::cbor::decode`, same struct, same field list as `qbuem::write` / `qbuem::read`.

---

## Why CBOR

CBOR is a standardized, self-describing binary format — think "binary JSON" with an IETF spec. qbuem-json chose it over a hand-rolled custom binary format for three reasons:

- **Cross-language by design.** The bytes carry their own type tags, so any RFC 8949 decoder reads them without a shared schema file. A C++ server can emit a frame that a TypeScript browser client decodes with `cbor-x` — no IDL, no codegen, no version drift between two hand-written codecs.
- **Smaller and faster than JSON.** No quotes, no field-name repetition bloat beyond the keys themselves, integers and floats stored in their native binary width. Typical payloads are **~40% smaller** and decode **several times faster** than the equivalent JSON (see [Benchmarks](./benchmarks#cbor-binary-codec)).
- **Zero new maintenance surface.** It reuses the existing field reflection and the same concept-based type dispatch as the JSON path. Registering a field once gets you JSON *and* CBOR; there is no second list to keep in sync.

> **Rule of thumb.** Use **JSON** for config files, logs, debugging, and human-facing or REST surfaces. Use **CBOR** for hot-path machine-to-machine traffic — real-time WebSocket frames, game state deltas, bandwidth-constrained mobile links, and on-device binary persistence.

---

## API

All CBOR entry points live in `namespace qbuem::cbor`. Bytes are carried in a `std::string` used as a byte container.

```cpp
// ── Encode ──────────────────────────────────────────────────────────────────
std::string  qbuem::cbor::encode(const T& obj);            // → new byte string
void         qbuem::cbor::encode_to(std::string& buf, const T& obj);  // append (reuses capacity)

// ── Decode ──────────────────────────────────────────────────────────────────
T            qbuem::cbor::decode<T>(std::string_view bytes);          // throws on bad input
T            qbuem::cbor::decode<T>(const uint8_t* data, size_t n);   // (ptr, len) overload
void         qbuem::cbor::decode_into<T>(std::string_view bytes, T& obj);  // fill existing object
```

- `decode<T>` throws [`qbuem::parse_error`](./errors) (with `.offset()`) on truncated, malformed, or type-mismatched input — it **never reads out of bounds**, so it is safe on untrusted network bytes.
- Trailing bytes after the top-level item are ignored, which is convenient for length-framed messages (decode the item, advance by the frame length).
- `encode_to` and `decode_into` reuse an existing buffer / object so a hot loop pays no per-message heap allocation.

### Hot-loop pattern

```cpp
std::string scratch;                 // reused across frames — keeps its capacity
for (const auto& delta : outgoing) {
    scratch.clear();
    qbuem::cbor::encode_to(scratch, delta);
    socket.send_binary(scratch);     // one encode, zero re-allocation
}
```

---

## Supported types

CBOR decode/encode covers the **same type set as the JSON engine** — register with `QBUEM_JSON_FIELDS` and these all work, nested arbitrarily:

| C++ type | CBOR representation |
|:---|:---|
| `bool` | simple `true` / `false` |
| `int*` / `uint*` (incl. `uint64_t`) | major 0 (unsigned) / major 1 (negative), smallest width |
| `float` / `double` | IEEE-754 (decode also accepts half / single / integer) |
| `std::string` | text string (major 3) |
| `std::optional<T>` | the value, or `null` when empty |
| `std::vector` / `std::list` / `std::deque` / sets | array (major 4) |
| `std::map` / `std::unordered_map` | map (major 5) |
| `std::array<T,N>` / `std::tuple` / `std::pair` | array (major 4) |
| `QBUEM_JSON_FIELDS` struct | map (major 5) keyed by field name |

`uint64_t` values with the top bit set round-trip exactly (the decoder reads the raw CBOR argument, with no `int64` narrowing).

**Generic types.** A class template can be registered once for all instantiations with [`QBUEM_JSON_FIELDS_TPL`](./mapping#generic-template-types) — handy for a generic envelope like `Message<T>`:

```cpp
template <typename T> struct Message { int64_t seq; T body; };
QBUEM_JSON_FIELDS_TPL((typename T), (Message<T>), seq, body)

auto bytes = qbuem::cbor::encode(Message<PlayerState>{ 7, state });   // any T
```

---

## Cross-language interop

A struct encoded by qbuem-json decodes directly in JavaScript/TypeScript with `cbor-x` — no schema:

```cpp
// C++ server
struct Tick { int64_t t; double px; int64_t qty; };
QBUEM_JSON_FIELDS(Tick, t, px, qty)

std::string frame = qbuem::cbor::encode(Tick{ 1718, 101.25, 500 });
ws.send_binary(frame);
```

```ts
// TypeScript browser client
import { decode } from 'cbor-x'

ws.onmessage = (ev) => {
  const tick = decode(new Uint8Array(ev.data))   // { t: 1718, px: 101.25, qty: 500 }
}
```

Because qbuem-json emits a CBOR **map keyed by field name**, the decoded object on the other side is a plain keyed object — field order, missing optional fields, and forward-compatible extra fields all behave the way any RFC 8949 reader expects.

---

## Safety on untrusted input

The decoder is built for bytes coming off a socket:

- **Every buffer advance is bounds-checked** — a truncated or hostile payload raises `qbuem::parse_error`, never an out-of-bounds read.
- **Recursion is depth-bounded** (1024) so a maliciously deep nesting can't exhaust the native stack.
- It is exercised by a dedicated **libFuzzer target** (`fuzz_cbor`) under ASan/UBSan in CI, in addition to the unit suite.

```cpp
auto safe_decode(std::string_view frame) -> std::optional<Msg> {
    try { return qbuem::cbor::decode<Msg>(frame); }
    catch (const qbuem::parse_error& e) {
        log_warn("bad CBOR frame at offset {}", e.offset());
        return std::nullopt;
    }
}
```

---

## See also

- [CBOR Binary Codec (theory)](../theory/cbor) — the RFC 8949 data model, the definite-length map choice, and the positional decode fast path.
- [Benchmarks → CBOR](./benchmarks#cbor-binary-codec) — measured encode/decode latency vs JSON and other CBOR libraries.
- [Object Mapping (Macros)](./mapping) — the `QBUEM_JSON_FIELDS` registration shared by both engines.
