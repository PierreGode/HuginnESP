import constants from './lib/constants'

/** The set of encodings supported by `Buffer` methods that convert between bytes and strings. */
type BufferEncoding =
  | 'ascii'
  | 'base64'
  | 'binary'
  | 'hex'
  | 'latin1'
  | 'ucs-2'
  | 'ucs2'
  | 'utf-16le'
  | 'utf-8'
  | 'utf16le'
  | 'utf8'

interface Buffer extends Uint8Array<ArrayBuffer> {
  /**
   * Compare this buffer's contents against `target` lexicographically, optionally comparing only
   * a sub-range of each, returning -1, 0, or 1 for sort ordering.
   * @param target - The buffer to compare against.
   * @param targetStart - Offset within `target` to start comparing from; defaults to `0`.
   * @param targetEnd - Offset within `target` (exclusive) to stop comparing at; defaults to
   * `target.byteLength`.
   * @param sourceStart - Offset within this buffer to start comparing from; defaults to `0`.
   * @param sourceEnd - Offset within this buffer (exclusive) to stop comparing at; defaults to
   * `this.byteLength`.
   * @throws {TypeError} thrown if `target` is not a view.
   * @throws {RangeError} thrown if an offset or length is not an integer, or if either view's
   * range lies outside its backing store.
   */
  compare(
    target: Buffer | ArrayBufferView,
    targetStart?: number,
    targetEnd?: number,
    sourceStart?: number,
    sourceEnd?: number
  ): number

  /**
   * Copy bytes from this buffer into `target`, returning the number of bytes copied. A target of
   * wider elements, such as a `Uint16Array`, receives the bytes it spans.
   * @param target - The buffer or view to copy into.
   * @param targetStart - Offset within `target` to start writing at; defaults to `0`.
   * @param sourceStart - Offset within this buffer to start copying from; defaults to `0`.
   * @param sourceEnd - Offset within this buffer (exclusive) to stop copying at; defaults to
   * `this.byteLength`.
   * @throws {TypeError} thrown if `target` is not a view.
   * @throws {RangeError} thrown if an offset is not an integer, is negative, or if `sourceStart`
   * is past the end of this buffer.
   */
  copy(
    target: Buffer | ArrayBufferView,
    targetStart?: number,
    sourceStart?: number,
    sourceEnd?: number
  ): number

  /**
   * Check whether this buffer and `target` have identical contents.
   * @param target - The buffer or view to compare this buffer's contents against.
   * @throws {TypeError} thrown if `target` is not a view.
   * @throws {RangeError} thrown if either view's range lies outside its backing store.
   */
  equals(target: Buffer | ArrayBufferView): boolean

  /**
   * Fill this buffer with `value`, repeating as needed, and return it.
   * @param value - The value to fill with — a string or view (repeated across the range), or a
   * number/boolean byte. A view fills with the bytes it spans.
   * @param encoding - Encoding used to interpret `value` when it's a string; defaults to `'utf8'`.
   * @throws {Error} thrown if the encoding is not a recognized `BufferEncoding`.
   * @throws {TypeError} thrown if `value` is not a value that can fill.
   * @throws {RangeError} thrown if an offset or length is not an integer.
   */
  fill(value: string, encoding?: BufferEncoding): this
  fill(value: string, offset?: number, encoding?: BufferEncoding): this
  fill(value: string, offset?: number, end?: number, encoding?: BufferEncoding): this
  fill(value: Buffer | ArrayBufferView | number | boolean, offset?: number, end?: number): this

  /**
   * Check whether this buffer contains `value`.
   * @param value - The value to search for — a string, view, number byte, or boolean.
   * @param encoding - Encoding used to interpret `value` when it's a string; defaults to `'utf8'`.
   * @throws {Error} thrown if the encoding is not a recognized `BufferEncoding`.
   * @throws {TypeError} thrown if `value` is not a string, view, number, or boolean.
   * @throws {RangeError} thrown if an offset or length is not an integer, or if either view's
   * range lies outside its backing store.
   */
  includes(value: string, encoding?: BufferEncoding): boolean
  includes(value: string, offset?: number, encoding?: BufferEncoding): boolean
  includes(value: Buffer | ArrayBufferView | number | boolean, offset?: number): boolean

  /**
   * Return the first index at which `value` occurs in this buffer, or `-1` if not found.
   * @param value - The value to search for — a string, view, number byte, or boolean.
   * @param encoding - Encoding used to interpret `value` when it's a string; defaults to `'utf8'`.
   * @throws {Error} thrown if the encoding is not a recognized `BufferEncoding`.
   * @throws {TypeError} thrown if `value` is not a string, view, number, or boolean.
   * @throws {RangeError} thrown if an offset or length is not an integer, or if either view's
   * range lies outside its backing store.
   */
  indexOf(value: string, encoding?: BufferEncoding): number
  indexOf(value: string, offset?: number, encoding?: BufferEncoding): number
  indexOf(value: Buffer | ArrayBufferView | number | boolean, offset?: number): number

  /**
   * Return the last index at which `value` occurs in this buffer, or `-1` if not found.
   * @param value - The value to search for — a string, view, number byte, or boolean.
   * @param encoding - Encoding used to interpret `value` when it's a string; defaults to `'utf8'`.
   * @throws {Error} thrown if the encoding is not a recognized `BufferEncoding`.
   * @throws {TypeError} thrown if `value` is not a string, view, number, or boolean.
   * @throws {RangeError} thrown if an offset or length is not an integer, or if either view's
   * range lies outside its backing store.
   */
  lastIndexOf(value: string, encoding?: BufferEncoding): number
  lastIndexOf(value: string, offset?: number, encoding?: BufferEncoding): number
  lastIndexOf(value: Buffer | ArrayBufferView | number | boolean, offset?: number): number

  /**
   * Swap the byte order of each 16-bit group in this buffer in place, and return it.
   * @throws {RangeError} thrown if the buffer's length is not a multiple of 2 bytes, or if its
   * range lies outside its backing store.
   */
  swap16(): this
  /**
   * Swap the byte order of each 32-bit group in this buffer in place, and return it.
   * @throws {RangeError} thrown if the buffer's length is not a multiple of 4 bytes, or if its
   * range lies outside its backing store.
   */
  swap32(): this
  /**
   * Swap the byte order of each 64-bit group in this buffer in place, and return it.
   * @throws {RangeError} thrown if the buffer's length is not a multiple of 8 bytes, or if its
   * range lies outside its backing store.
   */
  swap64(): this

  /**
   * Decode this buffer (or a slice of it) to a string using `encoding`.
   * @param encoding - Encoding used to decode the bytes; defaults to `'utf8'`.
   * @param start - Byte offset to start decoding from; defaults to `0`.
   * @param end - Byte offset (exclusive) to stop decoding at; defaults to `this.byteLength`.
   * @throws {Error} thrown if the encoding is not a recognized `BufferEncoding`.
   * @throws {RangeError} thrown if an offset or length is not an integer, if this buffer's range
   * lies outside its backing store, or if the decoded string would exceed
   * `Buffer.constants.MAX_STRING_LENGTH`.
   */
  toString(encoding?: BufferEncoding, start?: number, end?: number): string

  /**
   * Return an array of this buffer's bytes, used when the buffer is passed to `JSON.stringify()`.
   */
  toJSON(): number[]

  /**
   * Read a 64-bit big-endian double at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readDoubleBE(offset?: number): number
  /**
   * Read a 64-bit little-endian double at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readDoubleLE(offset?: number): number

  /**
   * Read a 32-bit big-endian float at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readFloatBE(offset?: number): number
  /**
   * Read a 32-bit little-endian float at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readFloatLE(offset?: number): number

  /**
   * Read a signed 8-bit integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readInt8(offset?: number): number

  /**
   * Read a signed 16-bit big-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readInt16BE(offset?: number): number
  /**
   * Read a signed 16-bit little-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readInt16LE(offset?: number): number

  /**
   * Read a signed 32-bit big-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readInt32BE(offset?: number): number
  /**
   * Read a signed 32-bit little-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readInt32LE(offset?: number): number

  /**
   * Read a signed, big-endian, `byteLength`-byte integer at `offset`.
   * @param offset - Byte offset to read from.
   * @param byteLength - Number of bytes to read, from `1` to `6`.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  readIntBE(offset: number, byteLength: number): number
  /**
   * Read a signed, little-endian, `byteLength`-byte integer at `offset`.
   * @param offset - Byte offset to read from.
   * @param byteLength - Number of bytes to read, from `1` to `6`.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  readIntLE(offset: number, byteLength: number): number

  /**
   * Read a signed 64-bit big-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readBigInt64BE(offset?: number): bigint
  /**
   * Read a signed 64-bit little-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readBigInt64LE(offset?: number): bigint

  /**
   * Read an unsigned 8-bit integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readUInt8(offset?: number): number
  /**
   * Read an unsigned 8-bit integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readUint8(offset?: number): number

  /**
   * Read an unsigned 16-bit big-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readUInt16BE(offset?: number): number
  /**
   * Read an unsigned 16-bit big-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readUint16BE(offset?: number): number
  /**
   * Read an unsigned 16-bit little-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readUInt16LE(offset?: number): number
  /**
   * Read an unsigned 16-bit little-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readUint16LE(offset?: number): number

  /**
   * Read an unsigned 32-bit big-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readUInt32BE(offset?: number): number
  /**
   * Read an unsigned 32-bit big-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readUint32BE(offset?: number): number
  /**
   * Read an unsigned 32-bit little-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readUInt32LE(offset?: number): number
  /**
   * Read an unsigned 32-bit little-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readUint32LE(offset?: number): number

  /**
   * Read an unsigned, big-endian, `byteLength`-byte integer at `offset`.
   * @param offset - Byte offset to read from.
   * @param byteLength - Number of bytes to read, from `1` to `6`.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  readUIntBE(offset: number, byteLength: number): number
  /**
   * Read an unsigned, big-endian, `byteLength`-byte integer at `offset`.
   * @param offset - Byte offset to read from.
   * @param byteLength - Number of bytes to read, from `1` to `6`.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  readUintBE(offset: number, byteLength: number): number
  /**
   * Read an unsigned, little-endian, `byteLength`-byte integer at `offset`.
   * @param offset - Byte offset to read from.
   * @param byteLength - Number of bytes to read, from `1` to `6`.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  readUIntLE(offset: number, byteLength: number): number
  /**
   * Read an unsigned, little-endian, `byteLength`-byte integer at `offset`.
   * @param offset - Byte offset to read from.
   * @param byteLength - Number of bytes to read, from `1` to `6`.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  readUintLE(offset: number, byteLength: number): number

  /**
   * Read an unsigned 64-bit big-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readBigUInt64BE(offset?: number): bigint
  /**
   * Read an unsigned 64-bit big-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readBigUint64BE(offset?: number): bigint
  /**
   * Read an unsigned 64-bit little-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readBigUInt64LE(offset?: number): bigint
  /**
   * Read an unsigned 64-bit little-endian integer at `offset`.
   * @param offset - Byte offset to read from; defaults to `0`.
   */
  readBigUint64LE(offset?: number): bigint

  /**
   * Write `string` into this buffer using `encoding`, returning the number of bytes written.
   * @param string - The string to write.
   * @param encoding - Encoding used to encode `string`; defaults to `'utf8'`.
   * @throws {Error} thrown if the encoding is not a recognized `BufferEncoding`, or if `string`
   * is not valid in it, as a hex or base64 string can fail to be.
   * @throws {TypeError} thrown if `string` is not a string.
   * @throws {RangeError} thrown if an offset or length is not an integer, or if this buffer's
   * range lies outside its backing store.
   */
  write(string: string, encoding?: BufferEncoding): number
  write(string: string, offset?: number, encoding?: BufferEncoding): number
  write(string: string, offset?: number, length?: number, encoding?: BufferEncoding): number

  /**
   * Write a 64-bit big-endian double at `offset`.
   * @param value - The double-precision number to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 8`, the offset immediately following the written value.
   */
  writeDoubleBE(value: number, offset?: number): number
  /**
   * Write a 64-bit little-endian double at `offset`.
   * @param value - The double-precision number to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 8`, the offset immediately following the written value.
   */
  writeDoubleLE(value: number, offset?: number): number

  /**
   * Write a 32-bit big-endian float at `offset`.
   * @param value - The single-precision number to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 4`, the offset immediately following the written value.
   */
  writeFloatBE(value: number, offset?: number): number
  /**
   * Write a 32-bit little-endian float at `offset`.
   * @param value - The single-precision number to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 4`, the offset immediately following the written value.
   */
  writeFloatLE(value: number, offset?: number): number

  /**
   * Write a signed 8-bit integer at `offset`.
   * @param value - The signed integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 1`, the offset immediately following the written value.
   */
  writeInt8(value: number, offset?: number): number

  /**
   * Write a signed 16-bit big-endian integer at `offset`.
   * @param value - The signed integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 2`, the offset immediately following the written value.
   */
  writeInt16BE(value: number, offset?: number): number
  /**
   * Write a signed 16-bit little-endian integer at `offset`.
   * @param value - The signed integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 2`, the offset immediately following the written value.
   */
  writeInt16LE(value: number, offset?: number): number

  /**
   * Write a signed 32-bit big-endian integer at `offset`.
   * @param value - The signed integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 4`, the offset immediately following the written value.
   */
  writeInt32BE(value: number, offset?: number): number
  /**
   * Write a signed 32-bit little-endian integer at `offset`.
   * @param value - The signed integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 4`, the offset immediately following the written value.
   */
  writeInt32LE(value: number, offset?: number): number

  /**
   * Write a signed, big-endian, `byteLength`-byte integer at `offset`.
   * @param value - The signed integer to write.
   * @param offset - Byte offset to write to.
   * @param byteLength - Number of bytes to write, from `1` to `6`.
   * @returns `offset + byteLength`, the offset immediately following the written value.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  writeIntBE(value: number, offset: number, byteLength: number): number
  /**
   * Write a signed, little-endian, `byteLength`-byte integer at `offset`.
   * @param value - The signed integer to write.
   * @param offset - Byte offset to write to.
   * @param byteLength - Number of bytes to write, from `1` to `6`.
   * @returns `offset + byteLength`, the offset immediately following the written value.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  writeIntLE(value: number, offset: number, byteLength: number): number

  /**
   * Write a signed 64-bit big-endian integer at `offset`.
   * @param value - The signed `bigint` to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 8`, the offset immediately following the written value.
   */
  writeBigInt64BE(value: bigint, offset?: number): number
  /**
   * Write a signed 64-bit little-endian integer at `offset`.
   * @param value - The signed `bigint` to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 8`, the offset immediately following the written value.
   */
  writeBigInt64LE(value: bigint, offset?: number): number

  /**
   * Write an unsigned 8-bit integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 1`, the offset immediately following the written value.
   */
  writeUInt8(value: number, offset?: number): number
  /**
   * Write an unsigned 8-bit integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 1`, the offset immediately following the written value.
   */
  writeUint8(value: number, offset?: number): number

  /**
   * Write an unsigned 16-bit big-endian integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 2`, the offset immediately following the written value.
   */
  writeUInt16BE(value: number, offset?: number): number
  /**
   * Write an unsigned 16-bit big-endian integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 2`, the offset immediately following the written value.
   */
  writeUint16BE(value: number, offset?: number): number
  /**
   * Write an unsigned 16-bit little-endian integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 2`, the offset immediately following the written value.
   */
  writeUInt16LE(value: number, offset?: number): number
  /**
   * Write an unsigned 16-bit little-endian integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 2`, the offset immediately following the written value.
   */
  writeUint16LE(value: number, offset?: number): number

  /**
   * Write an unsigned 32-bit big-endian integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 4`, the offset immediately following the written value.
   */
  writeUInt32BE(value: number, offset?: number): number
  /**
   * Write an unsigned 32-bit big-endian integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 4`, the offset immediately following the written value.
   */
  writeUint32BE(value: number, offset?: number): number
  /**
   * Write an unsigned 32-bit little-endian integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 4`, the offset immediately following the written value.
   */
  writeUInt32LE(value: number, offset?: number): number
  /**
   * Write an unsigned 32-bit little-endian integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 4`, the offset immediately following the written value.
   */
  writeUint32LE(value: number, offset?: number): number

  /**
   * Write an unsigned, big-endian, `byteLength`-byte integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to.
   * @param byteLength - Number of bytes to write, from `1` to `6`.
   * @returns `offset + byteLength`, the offset immediately following the written value.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  writeUIntBE(value: number, offset: number, byteLength: number): number
  /**
   * Write an unsigned, big-endian, `byteLength`-byte integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to.
   * @param byteLength - Number of bytes to write, from `1` to `6`.
   * @returns `offset + byteLength`, the offset immediately following the written value.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  writeUintBE(value: number, offset: number, byteLength: number): number
  /**
   * Write an unsigned, little-endian, `byteLength`-byte integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to.
   * @param byteLength - Number of bytes to write, from `1` to `6`.
   * @returns `offset + byteLength`, the offset immediately following the written value.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  writeUIntLE(value: number, offset: number, byteLength: number): number
  /**
   * Write an unsigned, little-endian, `byteLength`-byte integer at `offset`.
   * @param value - The unsigned integer to write.
   * @param offset - Byte offset to write to.
   * @param byteLength - Number of bytes to write, from `1` to `6`.
   * @returns `offset + byteLength`, the offset immediately following the written value.
   * @throws {RangeError} thrown if `byteLength` is not between `1` and `6`.
   */
  writeUintLE(value: number, offset: number, byteLength: number): number

  /**
   * Write an unsigned 64-bit big-endian integer at `offset`.
   * @param value - The unsigned `bigint` to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 8`, the offset immediately following the written value.
   */
  writeBigUint64BE(value: bigint, offset?: number): number
  /**
   * Write an unsigned 64-bit big-endian integer at `offset`.
   * @param value - The unsigned `bigint` to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 8`, the offset immediately following the written value.
   */
  writeBigUInt64BE(value: bigint, offset?: number): number
  /**
   * Write an unsigned 64-bit little-endian integer at `offset`.
   * @param value - The unsigned `bigint` to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 8`, the offset immediately following the written value.
   */
  writeBigUint64LE(value: bigint, offset?: number): number
  /**
   * Write an unsigned 64-bit little-endian integer at `offset`.
   * @param value - The unsigned `bigint` to write.
   * @param offset - Byte offset to write to; defaults to `0`.
   * @returns `offset + 8`, the offset immediately following the written value.
   */
  writeBigUInt64LE(value: bigint, offset?: number): number
}

declare class Buffer extends Uint8Array<ArrayBuffer> {
  /**
   * Create a `Buffer` viewing `arrayBuffer`, optionally starting at `offset` for `length` bytes.
   * @param arrayBuffer - The `ArrayBuffer` to view.
   * @param offset - Byte offset into `arrayBuffer` to start the view at; defaults to `0`.
   * @param length - Number of bytes to view from `offset`; defaults to the rest of `arrayBuffer`.
   * @throws {RangeError} thrown if the resulting length would exceed `Buffer.constants.MAX_LENGTH`.
   */
  constructor(arrayBuffer: ArrayBuffer, offset?: number, length?: number)
}

/** A fixed-length view of binary data, backed by an `ArrayBuffer` and extending `Uint8Array`. */
declare namespace Buffer {
  /**
   * The size, in bytes, of the internal buffer pool used to satisfy small allocations. Setting it
   * to `0` turns pooling off.
   * @throws {TypeError} thrown if assigned a value that is not a number.
   * @throws {RangeError} thrown if assigned a value that is not an integer between `0` and
   * `Buffer.constants.MAX_LENGTH`.
   */
  export let poolSize: number

  /**
   * Check whether `value` is a `Buffer`.
   * @param value - The value to check.
   */
  export function isBuffer(value: unknown): value is Buffer

  /**
   * Check whether `encoding` is a supported `BufferEncoding`.
   * @param encoding - The encoding name to check.
   */
  export function isEncoding(encoding: string): encoding is BufferEncoding

  /**
   * Check whether `buffer` contains only valid ASCII-encoded data.
   * @param buffer - The buffer or view to check.
   * @throws {TypeError} thrown if `buffer` is not a view.
   * @throws {RangeError} thrown if `buffer`'s range lies outside its backing store.
   */
  export function isASCII(buffer: Buffer | ArrayBufferView): boolean
  /**
   * Check whether `buffer` contains only valid ASCII-encoded data.
   * @param buffer - The buffer or view to check.
   * @throws {TypeError} thrown if `buffer` is not a view.
   * @throws {RangeError} thrown if `buffer`'s range lies outside its backing store.
   */
  export function isAscii(buffer: Buffer | ArrayBufferView): boolean

  /**
   * Check whether `buffer` contains only valid UTF-8-encoded data.
   * @param buffer - The buffer or view to check.
   * @throws {TypeError} thrown if `buffer` is not a view.
   * @throws {RangeError} thrown if `buffer`'s range lies outside its backing store.
   */
  export function isUTF8(buffer: Buffer | ArrayBufferView): boolean
  /**
   * Check whether `buffer` contains only valid UTF-8-encoded data.
   * @param buffer - The buffer or view to check.
   * @throws {TypeError} thrown if `buffer` is not a view.
   * @throws {RangeError} thrown if `buffer`'s range lies outside its backing store.
   */
  export function isUtf8(buffer: Buffer | ArrayBufferView): boolean

  /**
   * Allocate a new, zero-filled `Buffer` of `size` bytes, optionally filled with `fill`.
   * @param size - Number of bytes to allocate.
   * @param fill - Value to fill the buffer with — a string, view, number byte, or boolean.
   * @param encoding - Encoding used to interpret `fill` when it's a string; defaults to `'utf8'`.
   * @throws {RangeError} thrown if `size` is not an integer between `0` and
   * `Buffer.constants.MAX_LENGTH`.
   * @throws {TypeError} thrown if `size` is not a number, or `fill` is not a value that can fill.
   */
  export function alloc(size: number, fill: string, encoding?: BufferEncoding): Buffer
  export function alloc(size: number, fill?: Buffer | ArrayBufferView | number | boolean): Buffer

  /**
   * Allocate a new `Buffer` of `size` bytes without zeroing its contents first.
   * @param size - Number of bytes to allocate.
   * @throws {RangeError} thrown if `size` is not an integer between `0` and
   * `Buffer.constants.MAX_LENGTH`.
   * @throws {TypeError} thrown if `size` is not a number.
   */
  export function allocUnsafe(size: number): Buffer

  /**
   * Allocate a new `Buffer` of `size` bytes without zeroing its contents first, bypassing the
   * internal buffer pool.
   * @param size - Number of bytes to allocate.
   * @throws {RangeError} thrown if `size` is not an integer between `0` and
   * `Buffer.constants.MAX_LENGTH`.
   * @throws {TypeError} thrown if `size` is not a number.
   */
  export function allocUnsafeSlow(size: number): Buffer

  /**
   * Return the number of bytes `string` would occupy once encoded as `encoding`.
   * @param string - The value whose encoded length to measure — a string, or an
   * `ArrayBufferView`/`ArrayBufferLike` (whose own `byteLength` is returned unchanged).
   * @param encoding - Encoding used to measure `string` when it's a string; defaults to `'utf8'`.
   * @throws {Error} thrown if the encoding is not a recognized `BufferEncoding`.
   */
  export function byteLength(
    string: ArrayBufferView | ArrayBufferLike | string,
    encoding?: BufferEncoding
  ): number

  /**
   * Compare two buffers' contents lexicographically, returning -1, 0, or 1 for sort ordering.
   * @param a - The first buffer to compare.
   * @param b - The second buffer to compare.
   * @throws {TypeError} thrown if `a` or `b` is not a view.
   * @throws {RangeError} thrown if either view's range lies outside its backing store.
   */
  export function compare(a: Buffer | ArrayBufferView, b: Buffer | ArrayBufferView): number

  /**
   * Concatenate `buffers` into a single new `Buffer`, optionally truncated or zero-padded to
   * `length`.
   * @param buffers - The buffers to concatenate, in order.
   * @param length - Total byte length of the result; defaults to the sum of `buffers`' lengths.
   * @throws {RangeError} thrown if an offset or length is not an integer.
   */
  export function concat(buffers: Buffer[], length?: number): Buffer

  /**
   * Return `buffer` unchanged if it is already a `Buffer`, otherwise wrap its underlying
   * `ArrayBuffer` in a new `Buffer`.
   * @param buffer - The buffer or view to coerce into a `Buffer`.
   */
  export function coerce(buffer: Buffer | ArrayBufferView): Buffer

  /**
   * Copy the bytes of `view`, starting at `offset` for `length` elements, into a new `Buffer`.
   * @param view - The typed array (or other `ArrayBufferLike` view) to copy bytes from.
   * @param offset - Index of the first element to copy; defaults to `0`.
   * @param length - Number of elements to copy; defaults to the rest of `view`.
   * @throws {RangeError} thrown if `offset + length` exceeds `view.length`.
   * @throws {RangeError} thrown if an offset or length is not an integer.
   */
  export function copyBytesFrom(view: ArrayBufferLike, offset?: number, length?: number): Buffer

  /**
   * Create a new `Buffer` from an array, array-like, string, or `ArrayBuffer`.
   * @param data - The array, array-like, string, buffer, or `ArrayBuffer` to create a new `Buffer`
   * from.
   * @throws {Error} thrown if the encoding is not a recognized `BufferEncoding`, or if `string`
   * is not valid in it, as a hex or base64 string can fail to be.
   * @throws {RangeError} thrown if an offset or length is not an integer.
   */
  export function from(data: Iterable<number>): Buffer
  export function from(data: ArrayLike<number>): Buffer
  export function from(string: string, encoding?: BufferEncoding): Buffer
  export function from(arrayBuffer: ArrayBufferLike, offset?: number, length?: number): Buffer

  /**
   * Decode a base64-encoded string into a Latin-1 binary string.
   * @param data - The base64-encoded string to decode.
   */
  export function atob(data: unknown): string
  /**
   * Encode a Latin-1 binary string into a base64 string.
   * @param data - The value to encode; non-strings are coerced with `String()`.
   */
  export function btoa(data: unknown): string

  /**
   * Re-encode `buffer` from one encoding to another, returning a new `Buffer`.
   * @param buffer - The buffer to re-encode.
   * @param from - The encoding `buffer` is currently in.
   * @param to - The encoding to convert to.
   * @throws {Error} thrown if the encoding is not a recognized `BufferEncoding`.
   */
  export function transcode(buffer: Buffer, from: BufferEncoding, to: BufferEncoding): Buffer

  export { Buffer, type BufferEncoding, constants }
}

export = Buffer
