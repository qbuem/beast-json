// Node smoke test for the qbuem-json WASM module. Run after build.sh:
//   node test/smoke.mjs
import createQbuemJson from '../dist/qbuem_json.mjs';

const Q = await createQbuemJson();
let fails = 0;
const eq = (a, b, msg) => {
  if (a !== b) { console.log(`FAIL ${msg}\n  got: ${a}\n  exp: ${b}`); fails++; }
};

eq(Q.validate('{"a":1}'), true, 'validate ok');
eq(Q.validate('{"a":}'), false, 'validate bad');
eq(Q.validate('[1,2,3]'), true, 'validate array');

eq(Q.minify('{ "a" : 1 , "b":[1, 2] }'), '{"a":1,"b":[1,2]}', 'minify');
eq(Q.prettify('{"a":1}', 2), '{\n  "a": 1\n}', 'prettify');
eq(Q.canonicalize('{"b":1,"a":1.50}'), '{"a":1.5,"b":1}', 'canonicalize');

const doc = '{"store":{"book":[{"t":"A"},{"t":"B"}]},"n":[10,20,30]}';
eq(Q.query(doc, '$.store.book[*].t'), '["A","B"]', 'query wildcard');
eq(Q.query(doc, '$.n[-1]'), '[30]', 'query negative index');
eq(Q.query(doc, '$..t'), '["A","B"]', 'query recursive descent');
eq(Q.query(doc, '$.nope'), '[]', 'query no match');

let threw = false;
try { Q.minify('{bad}'); } catch (e) { threw = true; }
eq(threw, true, 'minify throws on bad input');

console.log(fails ? `\n${fails} FAILED` : 'ALL WASM SMOKE TESTS PASSED');
process.exit(fails ? 1 : 0);
