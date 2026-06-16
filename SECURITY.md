# Security Policy

## Supported Versions

| Version | Supported |
|:--------|:----------|
| 1.0.x (current) | ✅ Active |
| < 1.0 | ❌ Not supported |

## Reporting a Vulnerability

**Please do not open a public GitHub Issue for security vulnerabilities.**

To report a security vulnerability, email **security@qbuem.com** with:

- A description of the vulnerability and its potential impact
- Steps to reproduce (a minimal test case is ideal)
- Which version(s) are affected
- Any suggested mitigations if you have them

We will acknowledge receipt within **48 hours** and aim to provide a resolution
timeline within **7 days**.  Critical vulnerabilities affecting memory safety
(buffer overflows, use-after-free) are prioritised for same-day acknowledgement.

## Disclosure Policy

We follow a **coordinated disclosure** model:

1. You report the issue privately.
2. We investigate and develop a fix.
3. We release a patched version and credit you (unless you prefer anonymity).
4. We publish a security advisory on GitHub after the patch is available.

We ask that you give us **90 days** from acknowledgement before public
disclosure, in line with industry-standard responsible disclosure practices.

## Scope

In scope:

- Memory safety issues in the parser or serializer (heap/stack overflows,
  use-after-free, out-of-bounds reads/writes)
- Undefined behaviour that could be exploited (integer overflow, misaligned
  access, type punning violations)
- Denial-of-service via algorithmic complexity on adversarial input
- Incorrect RFC 8259 / RFC 6901 / RFC 6902 compliance that could lead to
  security-relevant misinterpretation

Out of scope:

- Performance regressions without security impact
- Build system issues (CMake, FetchContent)
- Documentation errors

## Security Hardening

All parser/serializer entry points are designed to treat their input as
**untrusted**. The hardening posture includes:

- **Memory safety** — depth caps on every recursive path (parser, validator,
  Nexus typed decode, pretty-printer) prevent stack exhaustion; inputs larger
  than 4 GiB are rejected up front (32-bit tape offsets); the zero-copy
  lifetime contract is enforced at **compile time** — passing a temporary
  `std::string` to a borrowing `parse*` overload is a compile error, not a
  silent use-after-free.
- **Denial of service** — recursion and output-size blow-ups are bounded;
  pathological inputs cannot drive unbounded memory growth.
- **Type confusion** — the Nexus engine verifies actual key bytes after its
  hash dispatch, so a hash-colliding untrusted key cannot be routed into the
  wrong struct field (field spoofing).
- **Encoding** — strict mode (`parse_strict` / `rfc8259::validate`) rejects
  malformed UTF-8 (overlong encodings, UTF-8-encoded surrogates, code points
  beyond U+10FFFF, lone continuation/truncated sequences) per RFC 8259 §8.1;
  `decoded()` substitutes U+FFFD for lone surrogates so its output is always
  valid UTF-8.
- **Diagnostics** — invalid input throws a typed `qbuem::parse_error`
  (a `std::runtime_error` with an `offset()` byte position).

These are continuously verified:

- **AddressSanitizer (ASan)** — heap/stack overflow, use-after-free, memory leaks
- **UndefinedBehaviorSanitizer (UBSan)** — integer overflow, null deref, misaligned
  access, invalid enum
- **ThreadSanitizer (TSan)** — data races, lock-order inversions

ASan/UBSan and TSan run on every pull request and push to `main` across
**x86_64, aarch64, and Apple Silicon** via the
[Sanitizers CI workflow](https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml).

Eleven [libFuzzer](https://llvm.org/docs/LibFuzzer.html) targets fuzz the parser,
serializer, and struct-mapping paths; nine run as a blocking
[Fuzz CI gate](https://github.com/qbuem/qbuem-json/actions/workflows/fuzz.yml) on
every change (with a deeper weekly run), including `fuzz_parse`, `fuzz_dom`,
`fuzz_nexus`, `fuzz_direct`, `fuzz_pmr`, `fuzz_patch`, `fuzz_api_stress`,
`fuzz_rfc8259`, and `fuzz_float`.

## Attribution

Security researchers who responsibly disclose vulnerabilities will be credited
in the release notes and, if they consent, in this file.
