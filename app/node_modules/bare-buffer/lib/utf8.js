const binding = require('../binding')
const checked = require('./checked')

exports.byteLength = function byteLength(string) {
  return checked(binding.byteLengthUTF8(string))
}

exports.toString = function toString(buffer, start = 0, end = buffer.byteLength) {
  return binding.toStringUTF8(buffer.buffer, buffer.byteOffset + start, end - start)
}

exports.write = function write(buffer, string, start = 0, end = buffer.byteLength) {
  return checked(binding.writeUTF8(buffer.buffer, buffer.byteOffset + start, end - start, string))
}

exports.validate = function validate(buffer) {
  return checked(binding.validateUTF8(buffer.buffer, buffer.byteOffset, buffer.byteLength)) === 1
}
