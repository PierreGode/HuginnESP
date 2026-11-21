// A typed callback cannot raise, so the bindings report an argument they reject
// with a result no successful call can produce. There is one sentinel per error
// the untyped callback raises directly, so that the error does not change once
// V8 takes the fast path.
const OUT_OF_BOUNDS = -2147483648
const INVALID_BUFFER = -2147483647
const INVALID_STRING = -2147483646

module.exports = function checked(result) {
  if (result === OUT_OF_BOUNDS) {
    throw new RangeError('View is out of bounds of its backing store')
  }

  if (result === INVALID_BUFFER) {
    throw new TypeError('Buffer must be an array buffer')
  }

  if (result === INVALID_STRING) {
    throw new TypeError('String must be a string')
  }

  return result
}
