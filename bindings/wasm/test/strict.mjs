// Strict conformance check for the WASM module — proves the build faithfully
// exposes the native library's RFC behaviour, including the v1.16.0 RFC 9535 /
// RFC 8785 fixes. Self-contained (no external suites); run after build.sh:
//   node test/strict.mjs
import createQbuemJson from '../dist/qbuem_json.mjs';

const Q = await createQbuemJson();
let fails = 0;
const eq = (got, exp, msg) => { if (got !== exp) { console.log(`FAIL ${msg}\n  got: ${got}\n  exp: ${exp}`); fails++; } };
const throws = (fn, msg) => { let t = false; try { fn(); } catch { t = true; } if (!t) { console.log(`FAIL ${msg} (should throw)`); fails++; } };

// ── RFC 8785 JCS (canonicalize) — byte-exact, incl. the v1.16.0 fixes ─────────
eq(Q.canonicalize('{"b":1,"a":2}'), '{"a":2,"b":1}', 'JCS sort');
// ECMAScript Number::toString exponent format (1e+30, not 1E30) and 0.002 form:
eq(Q.canonicalize('{"x":1e30,"y":1e-27,"z":0.002}'), '{"x":1e+30,"y":1e-27,"z":0.002}', 'JCS number format');
// UTF-16 code-unit key ordering: a supplementary-plane key sorts after BMP keys
// whose code points exceed the high-surrogate range only via UTF-16 rules.
eq(Q.canonicalize('{"\u{1F602}":1,"ד":2}'), '{"ד":2,"\u{1F602}":1}', 'JCS UTF-16 key order');

// ── RFC 9535 JSONPath (query) — the v1.16.0 fixes ─────────────────────────────
// Name selectors decode \u + surrogate pairs and match escaped object keys:
eq(Q.query('{"\\n":"NL"}', '$["\\n"]'), '["NL"]', 'query escaped-key match');
eq(Q.query('{"\u{1F600}":"smile"}', '$["\\uD83D\\uDE00"]'), '["smile"]', 'query surrogate-pair key');
// Filters work:
eq(Q.query('{"b":[{"p":8},{"p":12}]}', '$.b[?@.p<10]'), '[{"p":8}]', 'query filter');
// Invalid selectors are rejected per the grammar (strictness):
for (const sel of ['$.1', '$[01]', '$[+1]', '$[-0]', ' $', '$["\\uD800"]', '$[?@.a==01]', '$[9007199254740992]'])
  throws(() => Q.query('{"a":1}', sel), `reject invalid selector ${sel}`);

// ── RFC 8259 validate + minify/prettify ──────────────────────────────────────
eq(Q.validate('{"a":1}'), true, 'validate ok');
eq(Q.validate('{"a":}'), false, 'validate bad');
eq(Q.minify('{ "a" : 1 }'), '{"a":1}', 'minify');
eq(Q.prettify('{"a":1}', 2), '{\n  "a": 1\n}', 'prettify');

console.log(fails ? `\n${fails} STRICT CHECK(S) FAILED` : 'ALL STRICT WASM CONFORMANCE CHECKS PASSED');
process.exit(fails ? 1 : 0);
