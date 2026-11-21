const binding = require('../binding')
const checked = require('./checked')

exports.byteLength = function byteLength(string) {
  return string.length >>> 1
}

exports.toString = function toString(buffer, start = 0, end = buffer.byteLength) {
  return binding.toStringHex(buffer.buffer, buffer.byteOffset + start, end - start)
}

exports.write = function write(buffer, string, start = 0, end = buffer.byteLength) {
  const written = checked(
    binding.writeHex(buffer.buffer, buffer.byteOffset + start, end - start, string)
  )

  if (written < 0) throw new Error('Invalid input')

  return written
}
