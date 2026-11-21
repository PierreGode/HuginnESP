const binding = require('../binding')
const checked = require('./checked')

exports.byteLength = function byteLength(string) {
  let len = string.length

  if (string.charCodeAt(len - 1) === 0x3d) len--
  if (len > 1 && string.charCodeAt(len - 1) === 0x3d) len--

  return (len * 3) >>> 2
}

exports.toString = function toString(buffer, start = 0, end = buffer.byteLength) {
  return binding.toStringBase64URL(buffer.buffer, buffer.byteOffset + start, end - start)
}

exports.write = function write(buffer, string, start = 0, end = buffer.byteLength) {
  const written = checked(
    binding.writeBase64(buffer.buffer, buffer.byteOffset + start, end - start, string)
  )

  if (written < 0) throw new Error('Invalid input')

  return written
}
