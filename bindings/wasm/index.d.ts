// TypeScript definitions for the qbuem-json WebAssembly module.

export interface QbuemJson {
  /** True iff `json` is well-formed per RFC 8259 (strict UTF-8). Never throws. */
  validate(json: string): boolean;
  /** Compact re-serialization (whitespace stripped). Throws on invalid input. */
  minify(json: string): string;
  /** Pretty-print with `indent` spaces per level. Throws on invalid input. */
  prettify(json: string, indent: number): string;
  /**
   * Deterministic canonical form (sorted keys, shortest numbers) for hashing /
   * signing / content-addressing — RFC 8785 (JCS) structure. Strict-parses input.
   */
  canonicalize(json: string): string;
  /**
   * JSONPath (RFC 9535 structural selectors) query. Returns a JSON array string
   * of the selected values, in document order. Throws on a malformed query.
   * Filter expressions (`[?...]`) are not supported.
   */
  query(json: string, path: string): string;
}

/** Instantiate the WASM module. Resolves to the API once the runtime is ready. */
export default function createQbuemJson(): Promise<QbuemJson>;
