import * as buffer from '.'

type BufferConstructor = typeof buffer.Buffer

declare global {
  /** A fixed-length view of binary data, backed by an `ArrayBuffer` and extending `Uint8Array`. */
  type Buffer = buffer.Buffer

  const Buffer: BufferConstructor

  /**
   * Decode a base64-encoded string into a Latin-1 binary string.
   * @param data - The base64-encoded string to decode.
   */
  const atob: typeof buffer.atob
  /**
   * Encode a Latin-1 binary string into a base64 string.
   * @param data - The value to encode; non-strings are coerced with `String()`.
   */
  const btoa: typeof buffer.btoa
}
