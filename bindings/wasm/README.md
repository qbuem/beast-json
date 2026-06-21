# qbuem-json — WebAssembly

[qbuem-json](https://qbuem.com/qbuem-json/) compiled to WebAssembly. Runs the
native SIMD JSON engine in the browser and Node, exposing the operations the
platform doesn't provide itself: fast RFC 8259 validation, compact/pretty
re-serialization, **RFC 8785-style canonicalization** (for hashing & signing),
and **RFC 9535 JSONPath** querying — all over plain JSON strings.

## Build

Requires the [Emscripten SDK](https://emscripten.org) (`emcc`/`em++`) on `PATH`:

```bash
./build.sh          # → dist/qbuem_json.mjs + dist/qbuem_json.wasm
node test/smoke.mjs # run the smoke test
```

## Usage

```js
import createQbuemJson from '@qbuem/json-wasm';

const Q = await createQbuemJson();

Q.validate('{"a":1}');                       // true
Q.minify('{ "a" : 1 }');                      // '{"a":1}'
Q.prettify('{"a":1}', 2);                     // '{\n  "a": 1\n}'
Q.canonicalize('{"b":1,"a":1.50}');           // '{"a":1.5,"b":1}'  (sorted, shortest)

const doc = '{"store":{"book":[{"t":"A"},{"t":"B"}]}}';
Q.query(doc, '$.store.book[*].t');            // '["A","B"]'  (JSON array string)
Q.query(doc, '$..t');                         // '["A","B"]'  (recursive descent)
```

`minify` / `prettify` / `canonicalize` / `query` throw on invalid input or a
malformed query; `validate` returns a boolean and never throws.

## Why WASM here

JavaScript already has `JSON.parse`, so the value-add is the things it lacks:
**canonicalization** (deterministic bytes for signing/dedup) and **JSONPath**
(multi-match queries with wildcards, recursive descent, and slices) — plus a fast
SIMD validator for large payloads — all without pulling in several JS libraries.

## License

Apache-2.0.
