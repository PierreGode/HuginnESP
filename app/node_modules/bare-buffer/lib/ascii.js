const binding = require('../binding')
const checked = require('./checked')

exports.byteLength = function byteLength(string) {
  return string.length
}

exports.toString = function toString(buffer, start = 0, end = buffer.byteLength) {
  const offset = buffer.byteOffset + start
  const len = end - start

  // ASCII is Latin-1 with the high bit unset, so as long as no high bits are set
  // the native Latin-1 decoder produces the same string. Decoding through it
  // either way keeps the range checked in one place.
  const latin1 = binding.toStringLatin1(buffer.buffer, offset, len)

  if (exports.validate(buffer, offset, len)) return latin1

  let result = ''

  for (let i = 0, n = latin1.length; i < n; i++) {
    result += String.fromCharCode(latin1.charCodeAt(i) & 0x7f)
  }

  return result
}

// Encoding ASCII is equivalent to encoding Latin-1, both truncating each code
// unit to its low byte.
exports.write = function write(buffer, string, start = 0, end = buffer.byteLength) {
  return checked(binding.writeLatin1(buffer.buffer, buffer.byteOffset + start, end - start, string))
}

exports.validate = function validate(buffer, offset = buffer.byteOffset, len = buffer.byteLength) {
  return checked(binding.validateAscii(buffer.buffer, offset, len)) === 1
}
