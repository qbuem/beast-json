# CBOR Binary Codec

qbuem-json serializes the same `QBUEM_JSON_FIELDS` struct to **two** wire formats: human-readable JSON and binary [CBOR (RFC 8949)](https://www.rfc-editor.org/rfc/rfc8949.html). This page explains the CBOR data model, the wire-format choices, and how the codec is tuned to land near the theoretical floor while staying schema-less and bounds-checked.

For the practical API and a usage guide, see [CBOR (Binary Serialization)](../guide/cbor).

---

## The CBOR data model

CBOR is, in effect, a standardized binary JSON. Every data item begins with one **initial byte** whose high 3 bits are the *major type* and whose low 5 bits are the *additional information* (a small value, or a code for how many argument bytes follow).

| Major | Meaning | qbuem-json use |
|:---:|:---|:---|
| 0 | unsigned integer | `uint*`, non-negative `int*` |
| 1 | negative integer (encoded as `−1 − n`) | negative `int*` |
| 2 | byte string | — |
| 3 | text string | `std::string`, field-name keys |
| 4 | array | `vector` / `set` / `array` / `tuple` |
| 5 | map | `QBUEM_JSON_FIELDS` structs, `std::map` |
| 7 | simple / float | `bool`, `null`, `double` (`0xfb`) |

The argument is encoded in the **smallest** width that fits: values `< 24` ride in the initial byte itself; otherwise 1, 2, 4, or 8 big-endian bytes follow (additional info `24`–`27`). This "small value in the head byte" case is overwhelmingly the common one — every small integer, every short field-name length, every modest array/map count — and the codec's hot paths are built around it.

Because each item is **self-describing** (the type travels with the value), a CBOR byte stream can be decoded with no external schema. That is the property that lets a C++ server and a JavaScript client share bytes without sharing a generated codec.

---

## Wire-format choices

### Structs are definite-length maps keyed by field name

A `QBUEM_JSON_FIELDS` struct serializes to a CBOR **map** (major 5) whose keys are the field-name text strings. The field count is known at compile time, so the codec emits a **definite-length** header (`0xA0 | N` for `N < 24`) rather than an indefinite map (`0xBF … 0xFF`):

$$
\texttt{0xA7}\;[\;\texttt{"id"}\,\to\,42\;]\;[\;\texttt{"x"}\,\to\,-200\;]\;\dots
$$

The definite header is strictly better: it drops the trailing `break` byte and lets the decoder run a counted loop instead of a per-entry break check. It remains wire-compatible — the decoder accepts both definite and indefinite maps, as does any RFC 8949 reader.

Keying by field name (rather than packing values positionally) is a deliberate trade. A positional array would be smaller and faster, but it would require both endpoints to agree on a schema — exactly the coupling CBOR is meant to avoid. The map keeps the format **schema-less and self-describing**; the optimizations below claw back most of the cost that choice implies.

### Compile-time key blobs

On encode, each field name is turned into its CBOR-encoded form (text header + bytes) **at compile time** via `make_cbor_key<>()`, living in `.rodata`. Emitting a field key is then a single bulk `memcpy` of a ready-made blob — the binary analogue of the JSON FastWriter's precomputed `"key":` literal — instead of re-deriving the header and length per call.

---

## Where the time goes

The honest way to ask "is this fast enough?" is to decompose the cost. Encoding/decoding **112 `int64` fields** (16 entities × 7 fields), `-O3 -march=native`, Apple Silicon:

| | encode | decode |
|:---|---:|---:|
| **STRUCT** (map, keyed — the real format) | 514 ns | 570 ns |
| **ARRAY** (same ints, *no keys* — the floor) | 424 ns | 473 ns |
| **= key-dispatch + framing overhead** | **90 ns (17 %)** | **~100 ns (17 %)** |

Two conclusions fall out:

- **Encode is already near its floor.** 83 % of encode time is the irreducible work of writing the values; the remaining 17 % is the field-key bytes, which *must* be on the wire for a schema-less format. There is no dispatch to remove — the encoder already writes fields straight through in order.
- **Decode's headroom was the key dispatch.** Before tuning, decode spent over half its time reading each key, hashing it, and routing through a `switch` — the inherent cost of a keyed map. That is what the fast path below targets.

---

## Decode: the positional fast path

A general CBOR map decoder must, per entry, read the key, hash it, and dispatch — the keys could arrive in any order. But in practice they almost always arrive in **struct-declaration order**: qbuem-json's own encoder emits fields in order, and most encoders preserve insertion order.

So the decoder tries a **positional fast path** first. For each field, in declaration order, it confirms the expected next key with a single **non-destructive byte compare** (`CborReader::peek_key_eq`) and, on a match, decodes the value directly — *no hash, no `switch` indirect branch, no separate verify*:

```text
for each field f in declaration order:
    if next key (peeked, not consumed) == "f":
        consume key; decode value into obj.f
    else:
        bail → hand this entry and all the rest to the general loop
```

The first key that does **not** match the expected field flips the fast path off; that entry and every remaining entry fall through to the original hash-`switch` loop. So reordered, foreign, partial, or unknown-key maps decode **exactly** as before — only the common in-order case is accelerated, and the fallback guarantees correctness for everything else.

The peek is a *full* byte compare against the compile-time field name, so it is a complete anti-spoofing check on its own, independent of the hash.

### Supporting micro-architecture

Two smaller changes keep the per-field cost minimal:

- **`read_head` hot/cold split.** The "value `< 24` in the head byte" case (the common one) is ~5 inline instructions; the 1/2/4/8-byte argument tail is moved out-of-line so the inliner folds the hot path into the per-field loop.
- **Length-only verify for short keys.** For field names ≤ 8 bytes, the key hash *is* the raw little-endian bytes, so a hash match plus an equal length already proves byte-equality — the general-path verify skips a `memcmp`. Names ≥ 9 bytes (where the hash folds and can genuinely collide) keep the full compare.

Multi-byte integer heads and IEEE-754 doubles are read and written with `memcpy` + `__builtin_bswap{32,64}` rather than byte-at-a-time loops, consistent with the library's existing little-endian assumption.

---

## Result

| decode (112 int64 fields) | latency |
|:---|---:|
| baseline (general hash dispatch) | ~1110 ns |
| + hot/cold `read_head`, short-key verify, bswap | ~1005 ns |
| + positional fast path | **~570 ns** |

The all-scalar case now lands a hair above the **473 ns key-less floor** — i.e. close to the theoretical limit for reading these values at all, while keeping the cross-language map format, full bounds checking, and the spoofing-resistant verify. See [Benchmarks → CBOR](../guide/benchmarks#cbor-binary-codec) for end-to-end numbers and a comparison against other CBOR libraries.

Every byte advance in the decoder is bounds-checked and the recursion depth is capped at 1024, so the same code is safe on untrusted network input — validated by a dedicated libFuzzer target under ASan/UBSan.
