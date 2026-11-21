#include <assert.h>
#include <bare.h>
#include <base64.h>
#include <hex.h>
#include <js.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

// Results up to this length are encoded on the stack, so that the common case
// of a short hex or base64 string involves neither an allocation nor the
// bookkeeping of an external string.
#define BARE_BUFFER_STACK_STRING_MAX 1024

// A buffer may be viewed at any byte offset, so the code units of a UTF-16
// string are not necessarily aligned within its backing store. Spans that are
// not are staged through an aligned copy.
#define BARE_BUFFER_STACK_UTF16_MAX (BARE_BUFFER_STACK_STRING_MAX / sizeof(utf16_t))

// A typed callback cannot raise, so an argument it rejects is reported as a
// result no successful call can produce and JavaScript raises on it. There is
// one sentinel per error the untyped callback raises directly, so that the
// error does not change once V8 takes the fast path.
#define BARE_BUFFER_OUT_OF_BOUNDS  INT32_MIN
#define BARE_BUFFER_INVALID_BUFFER (INT32_MIN + 1)
#define BARE_BUFFER_INVALID_STRING (INT32_MIN + 2)

static inline bool
bare_buffer__is_aligned(const void *data) {
  return ((uintptr_t) data & (sizeof(utf16_t) - 1)) == 0;
}

static _Atomic size_t bare_buffer__max_arraybuffer_length;
static _Atomic size_t bare_buffer__max_string_length;

static inline void
bare_buffer__read_limits(js_env_t *env) {
  int err;

  js_platform_t *platform;
  err = js_get_env_platform(env, &platform);
  assert(err == 0);

  js_platform_limits_t limits = {
    .version = 0,
  };

  err = js_get_platform_limits(platform, &limits);
  assert(err == 0);

  atomic_store_explicit(&bare_buffer__max_arraybuffer_length, limits.arraybuffer_length, memory_order_relaxed);
  atomic_store_explicit(&bare_buffer__max_string_length, limits.string_length, memory_order_relaxed);
}

static inline size_t
bare_buffer__arraybuffer_limit(void) {
  return atomic_load_explicit(&bare_buffer__max_arraybuffer_length, memory_order_relaxed);
}

static inline size_t
bare_buffer__string_limit(void) {
  return atomic_load_explicit(&bare_buffer__max_string_length, memory_order_relaxed);
}

// Nothing between here and the raising helpers below raises an error. A typed
// callback is entered without a handle scope and V8 does not support reporting
// errors from one, so raising there aborts the process. An argument that does
// not check out is reported to the caller instead, and a condition that has to
// reach JavaScript is returned as a negative count for the caller to raise.

static inline bool
bare_buffer__is_buffer(js_env_t *env, js_value_t *value) {
  int err;

  bool is_arraybuffer;
  err = js_is_arraybuffer(env, value, &is_arraybuffer);
  assert(err == 0);

  if (is_arraybuffer) return true;

  bool is_shared;
  err = js_is_sharedarraybuffer(env, value, &is_shared);
  assert(err == 0);

  return is_shared;
}

static inline int
bare_buffer__get_info(js_env_t *env, js_value_t *buffer, void **data, size_t *len) {
  int err;

  bool is_arraybuffer;
  err = js_is_arraybuffer(env, buffer, &is_arraybuffer);
  assert(err == 0);

  if (is_arraybuffer) {
    err = js_get_arraybuffer_info(env, buffer, data, len);
    assert(err == 0);

    return 0;
  }

  bool is_shared;
  err = js_is_sharedarraybuffer(env, buffer, &is_shared);
  assert(err == 0);

  if (is_shared) {
    err = js_get_sharedarraybuffer_info(env, buffer, data, len);
    assert(err == 0);

    return 0;
  }

  return -1;
}

static inline bool
bare_buffer__is_string(js_env_t *env, js_value_t *value) {
  int err;

  bool is_string;
  err = js_is_string(env, value, &is_string);
  assert(err == 0);

  return is_string;
}

// Returns 0, or the sentinel for the argument that did not check out.
static inline int
bare_buffer__slice(js_env_t *env, js_value_t *buffer, int64_t offset, int64_t len, void **data) {
  int err;

  void *base;
  size_t byte_len;
  err = bare_buffer__get_info(env, buffer, &base, &byte_len);
  if (err < 0) return BARE_BUFFER_INVALID_BUFFER;

  if (offset < 0 || len < 0 || (uint64_t) offset + (uint64_t) len > byte_len) {
    return BARE_BUFFER_OUT_OF_BOUNDS;
  }

  // A detached or zero length store has no address, and the string and codec
  // APIs read a null destination as a request for the length a write would need
  // rather than as a write of no bytes.
  static uint8_t nowhere;

  // Only an empty store lacks an address, so the bounds check above has already
  // narrowed the span to nothing and the single byte is never reached.
  assert(base != NULL || len == 0);

  *data = base == NULL ? &nowhere : (uint8_t *) base + offset;

  return 0;
}

static inline int
bare_buffer__alloc(size_t len, void **result) {
  void *data = malloc(len);

  if (data == NULL) return -1;

  *result = data;

  return 0;
}

static int64_t
bare_buffer__write_utf8(js_env_t *env, js_value_t *string, utf8_t *buf, int64_t len) {
  int err;

  size_t str_len;
  err = js_get_value_string_utf8(env, string, NULL, 0, &str_len);
  assert(err == 0);

  size_t written;

  if (str_len <= (size_t) len) {
    err = js_get_value_string_utf8(env, string, buf, str_len, &written);
    assert(err == 0);

    return written;
  }

  // A truncated write stops on a code point boundary, so it leaves at most the
  // three bytes of an incomplete sequence. Keeping those is enough to undo the
  // null terminator, which can only land among them.
  size_t tail_at = (size_t) len - (len < 3 ? (size_t) len : 3);

  utf8_t tail[3];
  memcpy(tail, &buf[tail_at], (size_t) len - tail_at);

  err = js_get_value_string_utf8(env, string, buf, len, &written);
  assert(err == 0);

  if (written < (size_t) len && written >= tail_at) {
    memcpy(&buf[written], &tail[written - tail_at], (size_t) len - written);
  }

  return written;
}

static int64_t
bare_buffer__write_latin1(js_env_t *env, js_value_t *string, latin1_t *buf, int64_t len) {
  int err;

  size_t str_len;
  err = js_get_value_string_latin1(env, string, NULL, 0, &str_len);
  assert(err == 0);

  if (str_len > (size_t) len) str_len = len;

  size_t written;
  err = js_get_value_string_latin1(env, string, buf, str_len, &written);
  assert(err == 0);

  return written;
}

static int64_t
bare_buffer__write_utf16le(js_env_t *env, js_value_t *string, uint8_t *bytes, int64_t len) {
  int err;

  size_t str_len;
  err = js_get_value_string_utf16le(env, string, NULL, 0, &str_len);
  assert(err == 0);

  size_t capacity = (size_t) len / sizeof(utf16_t);

  if (str_len > capacity) str_len = capacity;

  size_t written;

  if (bare_buffer__is_aligned(bytes)) {
    err = js_get_value_string_utf16le(env, string, (utf16_t *) bytes, str_len, &written);
    assert(err == 0);

    return written * sizeof(utf16_t);
  }

  utf16_t stack[BARE_BUFFER_STACK_UTF16_MAX];
  utf16_t *str = stack;

  if (str_len > BARE_BUFFER_STACK_UTF16_MAX) {
    err = bare_buffer__alloc(str_len * sizeof(utf16_t), (void **) &str);
    if (err < 0) return -1;
  }

  err = js_get_value_string_utf16le(env, string, str, str_len, &written);
  assert(err == 0);

  memcpy(bytes, str, written * sizeof(utf16_t));

  if (str != stack) free(str);

  return written * sizeof(utf16_t);
}

static int64_t
bare_buffer__write_base64(js_env_t *env, js_value_t *string, utf8_t *buf, int64_t len) {
  int err;

  js_string_encoding_t encoding;
  const void *str;
  size_t str_len;

  js_string_view_t *str_view;
  err = js_get_string_view(env, string, &encoding, &str, &str_len, &str_view);
  assert(err == 0);

  size_t written = len;

  if (encoding == js_utf16le) {
    err = base64_decode_utf16le(str, str_len, buf, &written);
  } else {
    err = base64_decode_utf8(str, str_len, buf, &written);
  }

  int64_t decoded = err == 0 ? (int64_t) written : -1;

  err = js_release_string_view(env, str_view);
  assert(err == 0);

  return decoded;
}

static int64_t
bare_buffer__write_hex(js_env_t *env, js_value_t *string, utf8_t *buf, int64_t len) {
  int err;

  js_string_encoding_t encoding;
  const void *str;
  size_t str_len;

  js_string_view_t *str_view;
  err = js_get_string_view(env, string, &encoding, &str, &str_len, &str_view);
  assert(err == 0);

  size_t written = len;

  if (encoding == js_utf16le) {
    err = hex_decode_utf16le(str, str_len, buf, &written);
  } else {
    err = hex_decode_utf8(str, str_len, buf, &written);
  }

  int64_t decoded = err == 0 ? (int64_t) written : -1;

  err = js_release_string_view(env, str_view);
  assert(err == 0);

  return decoded;
}

static bool
bare_buffer__check_buffer(js_env_t *env, js_value_t *value) {
  int err;

  if (bare_buffer__is_buffer(env, value)) return true;

  err = js_throw_type_error(env, NULL, "Buffer must be an array buffer");
  assert(err == 0);

  return false;
}

static bool
bare_buffer__check_number(js_env_t *env, js_value_t *value, const char *message) {
  int err;

  bool is_number;
  err = js_is_number(env, value, &is_number);
  assert(err == 0);

  if (!is_number) {
    err = js_throw_type_error(env, NULL, message);
    assert(err == 0);
  }

  return is_number;
}

static bool
bare_buffer__check_string(js_env_t *env, js_value_t *value, const char *message) {
  int err;

  if (bare_buffer__is_string(env, value)) return true;

  err = js_throw_type_error(env, NULL, message);
  assert(err == 0);

  return false;
}

static js_value_t *
bare_buffer__view_out_of_bounds(js_env_t *env) {
  int err;

  err = js_throw_range_error(env, NULL, "View is out of bounds of its backing store");
  assert(err == 0);

  return NULL;
}

static js_value_t *
bare_buffer__out_of_memory(js_env_t *env) {
  int err;

  err = js_throw_error(env, NULL, "Out of memory");
  assert(err == 0);

  return NULL;
}

static js_value_t *
bare_buffer__invalid_string_length(js_env_t *env) {
  int err;

  err = js_throw_range_error(env, NULL, "Invalid string length");
  assert(err == 0);

  return NULL;
}

static inline int64_t
bare_buffer__to_int64(js_env_t *env, js_value_t *value) {
  int err;

  int64_t result;
  err = js_get_value_int64(env, value, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_buffer_alloc(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_number(env, argv[0], "Length must be a number")) return NULL;

  int64_t len = bare_buffer__to_int64(env, argv[0]);

  if (len < 0) {
    err = js_throw_range_error(env, NULL, "Length must not be negative");
    assert(err == 0);

    return NULL;
  }

  if ((uint64_t) len > bare_buffer__arraybuffer_limit()) {
    err = js_throw_range_error(env, NULL, "Length must not exceed the maximum array buffer length");
    assert(err == 0);

    return NULL;
  }

  js_value_t *result;
  err = js_create_arraybuffer(env, len, NULL, &result);
  if (err < 0) return NULL;

  return result;
}

static js_value_t *
bare_buffer_alloc_unsafe(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_number(env, argv[0], "Length must be a number")) return NULL;

  int64_t len = bare_buffer__to_int64(env, argv[0]);

  if (len < 0) {
    err = js_throw_range_error(env, NULL, "Length must not be negative");
    assert(err == 0);

    return NULL;
  }

  if ((uint64_t) len > bare_buffer__arraybuffer_limit()) {
    err = js_throw_range_error(env, NULL, "Length must not exceed the maximum array buffer length");
    assert(err == 0);

    return NULL;
  }

  js_value_t *result;
  err = js_create_unsafe_arraybuffer(env, len, NULL, &result);
  if (err < 0) return NULL;

  return result;
}

static int64_t
bare_buffer_typed_byte_length_utf8(js_value_t *receiver, js_value_t *str, js_typed_callback_info_t *info) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  if (!bare_buffer__is_string(env, str)) return BARE_BUFFER_INVALID_STRING;

  size_t str_len;
  err = js_get_value_string_utf8(env, str, NULL, 0, &str_len);
  assert(err == 0);

  return str_len;
}

static js_value_t *
bare_buffer_byte_length_utf8(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_string(env, argv[0], "String must be a string")) return NULL;

  size_t str_len;
  err = js_get_value_string_utf8(env, argv[0], NULL, 0, &str_len);
  assert(err == 0);

  js_value_t *result;
  err = js_create_int64(env, str_len, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_buffer_to_string_utf8(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  utf8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);
  if (err < 0) return bare_buffer__view_out_of_bounds(env);

  js_value_t *result;
  err = js_create_string_utf8(env, buf, len, &result);
  if (err < 0) return NULL;

  return result;
}

static int32_t
bare_buffer_typed_validate_utf8(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  utf8_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  return utf8_validate(buf, len);
}

static js_value_t *
bare_buffer_validate_utf8(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  utf8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  int32_t valid = err == 0 ? utf8_validate(buf, len) : err;

  js_value_t *result;
  err = js_create_int32(env, valid, &result);
  assert(err == 0);

  return result;
}

static int64_t
bare_buffer_typed_write_utf8(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_value_t *string,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  if (!bare_buffer__is_buffer(env, handle)) return BARE_BUFFER_INVALID_BUFFER;
  if (!bare_buffer__is_string(env, string)) return BARE_BUFFER_INVALID_STRING;

  utf8_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  return bare_buffer__write_utf8(env, string, buf, len);
}

static js_value_t *
bare_buffer_write_utf8(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 4;
  js_value_t *argv[4];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;
  if (!bare_buffer__check_string(env, argv[3], "String must be a string")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  utf8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  int64_t written = err == 0 ? bare_buffer__write_utf8(env, argv[3], buf, len) : err;

  js_value_t *result;
  err = js_create_int64(env, written, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_buffer_to_string_utf16le(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  uint8_t *bytes;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &bytes);
  if (err < 0) return bare_buffer__view_out_of_bounds(env);

  size_t str_len = (size_t) len / sizeof(utf16_t);

  if (str_len > bare_buffer__string_limit()) {
    return bare_buffer__invalid_string_length(env);
  }

  js_value_t *result;

  if (bare_buffer__is_aligned(bytes)) {
    err = js_create_string_utf16le(env, (utf16_t *) bytes, str_len, &result);
    if (err < 0) return NULL;

    return result;
  }

  utf16_t stack[BARE_BUFFER_STACK_UTF16_MAX];
  utf16_t *str = stack;

  if (str_len > BARE_BUFFER_STACK_UTF16_MAX) {
    err = bare_buffer__alloc(str_len * sizeof(utf16_t), (void **) &str);
    if (err < 0) return bare_buffer__out_of_memory(env);
  }

  memcpy(str, bytes, str_len * sizeof(utf16_t));

  err = js_create_string_utf16le(env, str, str_len, &result);

  if (str != stack) free(str);

  if (err < 0) return NULL;

  return result;
}

static int64_t
bare_buffer_typed_write_utf16le(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_value_t *string,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  if (!bare_buffer__is_buffer(env, handle)) return BARE_BUFFER_INVALID_BUFFER;
  if (!bare_buffer__is_string(env, string)) return BARE_BUFFER_INVALID_STRING;

  uint8_t *bytes;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &bytes);
  if (err != 0) return err;

  return bare_buffer__write_utf16le(env, string, bytes, len);
}

static js_value_t *
bare_buffer_write_utf16le(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 4;
  js_value_t *argv[4];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;
  if (!bare_buffer__check_string(env, argv[3], "String must be a string")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  uint8_t *bytes;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &bytes);

  int64_t written = err == 0 ? bare_buffer__write_utf16le(env, argv[3], bytes, len) : err;

  js_value_t *result;
  err = js_create_int64(env, written, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_buffer_to_string_latin1(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  latin1_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);
  if (err < 0) return bare_buffer__view_out_of_bounds(env);

  js_value_t *result;
  err = js_create_string_latin1(env, buf, len, &result);
  if (err < 0) return NULL;

  return result;
}

static int64_t
bare_buffer_typed_write_latin1(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_value_t *string,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  if (!bare_buffer__is_buffer(env, handle)) return BARE_BUFFER_INVALID_BUFFER;
  if (!bare_buffer__is_string(env, string)) return BARE_BUFFER_INVALID_STRING;

  latin1_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  return bare_buffer__write_latin1(env, string, buf, len);
}

static js_value_t *
bare_buffer_write_latin1(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 4;
  js_value_t *argv[4];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;
  if (!bare_buffer__check_string(env, argv[3], "String must be a string")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  latin1_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  int64_t written = err == 0 ? bare_buffer__write_latin1(env, argv[3], buf, len) : err;

  js_value_t *result;
  err = js_create_int64(env, written, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_buffer_to_string_base64(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  utf8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);
  if (err < 0) return bare_buffer__view_out_of_bounds(env);

  size_t str_len;
  err = base64_encode_utf8(buf, len, NULL, &str_len);
  assert(err == 0);

  if (str_len > bare_buffer__string_limit()) {
    return bare_buffer__invalid_string_length(env);
  }

  utf8_t stack[BARE_BUFFER_STACK_STRING_MAX];
  utf8_t *str = stack;

  if (str_len > sizeof(stack)) {
    err = bare_buffer__alloc(str_len, (void **) &str);
    if (err < 0) return bare_buffer__out_of_memory(env);
  }

  err = base64_encode_utf8(buf, len, str, &str_len);
  assert(err == 0);

  js_value_t *result;
  err = js_create_string_latin1(env, str, str_len, &result);

  if (str != stack) free(str);

  if (err < 0) return NULL;

  return result;
}

static js_value_t *
bare_buffer_to_string_base64url(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  utf8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);
  if (err < 0) return bare_buffer__view_out_of_bounds(env);

  size_t str_len;
  err = base64url_encode_utf8(buf, len, NULL, &str_len);
  assert(err == 0);

  if (str_len > bare_buffer__string_limit()) {
    return bare_buffer__invalid_string_length(env);
  }

  utf8_t stack[BARE_BUFFER_STACK_STRING_MAX];
  utf8_t *str = stack;

  if (str_len > sizeof(stack)) {
    err = bare_buffer__alloc(str_len, (void **) &str);
    if (err < 0) return bare_buffer__out_of_memory(env);
  }

  err = base64url_encode_utf8(buf, len, str, &str_len);
  assert(err == 0);

  js_value_t *result;
  err = js_create_string_latin1(env, str, str_len, &result);

  if (str != stack) free(str);

  if (err < 0) return NULL;

  return result;
}

static int64_t
bare_buffer_typed_write_base64(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_value_t *string,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  if (!bare_buffer__is_buffer(env, handle)) return BARE_BUFFER_INVALID_BUFFER;
  if (!bare_buffer__is_string(env, string)) return BARE_BUFFER_INVALID_STRING;

  utf8_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  return bare_buffer__write_base64(env, string, buf, len);
}

static js_value_t *
bare_buffer_write_base64(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 4;
  js_value_t *argv[4];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;
  if (!bare_buffer__check_string(env, argv[3], "String must be a string")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  utf8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  int64_t decoded = err == 0 ? bare_buffer__write_base64(env, argv[3], buf, len) : err;

  js_value_t *result;
  err = js_create_int64(env, decoded, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_buffer_to_string_hex(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  utf8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);
  if (err < 0) return bare_buffer__view_out_of_bounds(env);

  size_t str_len;
  err = hex_encode_utf8(buf, len, NULL, &str_len);
  assert(err == 0);

  if (str_len > bare_buffer__string_limit()) {
    return bare_buffer__invalid_string_length(env);
  }

  utf8_t stack[BARE_BUFFER_STACK_STRING_MAX];
  utf8_t *str = stack;

  if (str_len > sizeof(stack)) {
    err = bare_buffer__alloc(str_len, (void **) &str);
    if (err < 0) return bare_buffer__out_of_memory(env);
  }

  err = hex_encode_utf8(buf, len, str, &str_len);
  assert(err == 0);

  js_value_t *result;
  err = js_create_string_latin1(env, str, str_len, &result);

  if (str != stack) free(str);

  if (err < 0) return NULL;

  return result;
}

static int64_t
bare_buffer_typed_write_hex(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_value_t *string,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  if (!bare_buffer__is_buffer(env, handle)) return BARE_BUFFER_INVALID_BUFFER;
  if (!bare_buffer__is_string(env, string)) return BARE_BUFFER_INVALID_STRING;

  utf8_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  return bare_buffer__write_hex(env, string, buf, len);
}

static js_value_t *
bare_buffer_write_hex(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 4;
  js_value_t *argv[4];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;
  if (!bare_buffer__check_string(env, argv[3], "String must be a string")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  utf8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  int64_t decoded = err == 0 ? bare_buffer__write_hex(env, argv[3], buf, len) : err;

  js_value_t *result;
  err = js_create_int64(env, decoded, &result);
  assert(err == 0);

  return result;
}

static int32_t
bare_buffer_typed_validate_ascii(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  ascii_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  return ascii_validate(buf, len);
}

static js_value_t *
bare_buffer_validate_ascii(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  ascii_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  int32_t valid = err == 0 ? ascii_validate(buf, len) : err;

  js_value_t *result;
  err = js_create_int32(env, valid, &result);
  assert(err == 0);

  return result;
}

static inline void
bare_buffer__swap16(uint8_t *data, size_t len) {
  for (size_t i = 0; i + 1 < len; i += 2) {
    uint8_t a = data[i];
    data[i] = data[i + 1];
    data[i + 1] = a;
  }
}

static inline void
bare_buffer__swap32(uint8_t *data, size_t len) {
  for (size_t i = 0; i + 3 < len; i += 4) {
    uint8_t a = data[i];
    uint8_t b = data[i + 1];
    data[i] = data[i + 3];
    data[i + 1] = data[i + 2];
    data[i + 2] = b;
    data[i + 3] = a;
  }
}

static inline void
bare_buffer__swap64(uint8_t *data, size_t len) {
  for (size_t i = 0; i + 7 < len; i += 8) {
    for (size_t j = 0; j < 4; j++) {
      uint8_t a = data[i + j];
      data[i + j] = data[i + 7 - j];
      data[i + 7 - j] = a;
    }
  }
}

static const uint8_t *
bare_buffer__memrchr(const uint8_t *data, uint8_t c, size_t len) {
  const size_t chunk_len = 512;

  while (len > 0) {
    size_t take = len < chunk_len ? len : chunk_len;

    const uint8_t *chunk = &data[len - take];

    // When matches are dense the last one is within a few bytes of the end,
    // which is far cheaper to find by looking than by narrowing.
    size_t probe = take < 8 ? take : 8;

    for (size_t i = 0; i < probe; i++) {
      if (chunk[take - 1 - i] == c) return &chunk[take - 1 - i];
    }

    if (memchr(chunk, c, take - probe) != NULL) {
      size_t lo = 0;
      size_t hi = take - probe;

      while (hi - lo > 1) {
        size_t mid = lo + (hi - lo) / 2;

        if (memchr(&chunk[mid], c, hi - mid) != NULL) lo = mid;
        else hi = mid;
      }

      return &chunk[lo];
    }

    len -= take;
  }

  return NULL;
}

static int64_t
bare_buffer__index_of(const uint8_t *data, size_t len, const uint8_t *needle, size_t needle_len, int64_t start) {
  if (needle_len == 0 || needle_len > len) return -1;

  // Compared before narrowing, so that a start past the end cannot wrap into a
  // valid position on a target where size_t is narrower than int64_t.
  if (start < 0 || (uint64_t) start > len - needle_len) return -1;

  size_t from = (size_t) start;

  if (needle_len == 1) {
    const uint8_t *at = memchr(&data[from], needle[0], len - from);

    return at == NULL ? -1 : (int64_t) (at - data);
  }

  size_t end = len - needle_len + 1;
  size_t i = from;
  size_t rejected = 0;

  while (i < end) {
    const uint8_t *at = memchr(&data[i], needle[0], end - i);

    if (at == NULL) return -1;

    i = (size_t) (at - data);

    if (memcmp(&data[i + 1], &needle[1], needle_len - 1) == 0) return (int64_t) i;

    i++;

    if (++rejected == 32) break;
  }

  if (i >= end) return -1;

  size_t skip[256];

  for (size_t j = 0; j < 256; j++) {
    skip[j] = needle_len;
  }

  for (size_t j = 0; j < needle_len - 1; j++) {
    skip[needle[j]] = needle_len - 1 - j;
  }

  size_t last = needle_len - 1;

  while (i < end) {
    if (data[i + last] == needle[last] && memcmp(&data[i], needle, last) == 0) {
      return (int64_t) i;
    }

    i += skip[data[i + last]];
  }

  return -1;
}

static int64_t
bare_buffer__last_index_of(const uint8_t *data, size_t len, const uint8_t *needle, size_t needle_len, int64_t start) {
  if (needle_len == 0 || needle_len > len) return -1;

  // A start before the first byte names no position, the same reading that has
  // the forward search report no match for a start past the last one.
  if (start < 0) return -1;

  size_t last = len - needle_len;
  size_t from = (uint64_t) start > last ? last : (size_t) start;

  if (needle_len == 1) {
    const uint8_t *at = bare_buffer__memrchr(data, needle[0], from + 1);

    return at == NULL ? -1 : (int64_t) (at - data);
  }

  size_t i = from;
  size_t rejected = 0;

  while (rejected < 32) {
    const uint8_t *at = bare_buffer__memrchr(data, needle[0], i + 1);

    if (at == NULL) return -1;

    i = (size_t) (at - data);

    if (memcmp(&data[i + 1], &needle[1], needle_len - 1) == 0) return (int64_t) i;

    if (i == 0) return -1;

    i--;
    rejected++;
  }

  size_t skip[256];

  for (size_t j = 0; j < 256; j++) {
    skip[j] = needle_len;
  }

  for (size_t j = needle_len - 1; j >= 1; j--) {
    skip[needle[j]] = j;
  }

  while (true) {
    if (data[i] == needle[0] && memcmp(&data[i + 1], &needle[1], needle_len - 1) == 0) {
      return (int64_t) i;
    }

    size_t shift = skip[data[i]];

    if (i < shift) return -1;

    i -= shift;
  }
}

static inline int
bare_buffer__memcmp(void *a, size_t a_len, void *b, size_t b_len) {
  int r = memcmp(a, b, a_len < b_len ? a_len : b_len);

  if (r == 0) {
    if (a_len < b_len) return -1;
    if (a_len > b_len) return 1;
    return 0;
  }

  return r < 0 ? -1 : 1;
}

static int32_t
bare_buffer_typed_compare(
  js_value_t *receiver,
  js_value_t *a_handle,
  int64_t a_offset,
  int64_t a_len,
  js_value_t *b_handle,
  int64_t b_offset,
  int64_t b_len,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  if (!bare_buffer__is_buffer(env, a_handle)) return BARE_BUFFER_INVALID_BUFFER;
  if (!bare_buffer__is_buffer(env, b_handle)) return BARE_BUFFER_INVALID_BUFFER;

  uint8_t *a;
  err = bare_buffer__slice(env, a_handle, a_offset, a_len, (void **) &a);
  if (err != 0) return err;

  uint8_t *b;
  err = bare_buffer__slice(env, b_handle, b_offset, b_len, (void **) &b);
  if (err != 0) return err;

  return bare_buffer__memcmp(a, a_len, b, b_len);
}

static js_value_t *
bare_buffer_compare(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 6;
  js_value_t *argv[6];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;
  if (!bare_buffer__check_buffer(env, argv[3])) return NULL;
  if (!bare_buffer__check_number(env, argv[4], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[5], "Length must be a number")) return NULL;

  int64_t a_offset = bare_buffer__to_int64(env, argv[1]);
  int64_t a_len = bare_buffer__to_int64(env, argv[2]);
  int64_t b_offset = bare_buffer__to_int64(env, argv[4]);
  int64_t b_len = bare_buffer__to_int64(env, argv[5]);

  uint8_t *a;
  err = bare_buffer__slice(env, argv[0], a_offset, a_len, (void **) &a);

  int32_t comparison = err;

  if (err == 0) {
    uint8_t *b;
    err = bare_buffer__slice(env, argv[3], b_offset, b_len, (void **) &b);

    comparison = err == 0 ? bare_buffer__memcmp(a, a_len, b, b_len) : err;
  }

  js_value_t *result;
  err = js_create_int32(env, comparison, &result);
  assert(err == 0);

  return result;
}

static int32_t
bare_buffer_typed_swap16(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  uint8_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  bare_buffer__swap16(buf, len);

  return 0;
}

static js_value_t *
bare_buffer_swap16(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  uint8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  if (err == 0) bare_buffer__swap16(buf, len);

  int32_t swapped = err;

  js_value_t *result;
  err = js_create_int32(env, swapped, &result);
  assert(err == 0);

  return result;
}

static int32_t
bare_buffer_typed_swap32(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  uint8_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  bare_buffer__swap32(buf, len);

  return 0;
}

static js_value_t *
bare_buffer_swap32(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  uint8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  if (err == 0) bare_buffer__swap32(buf, len);

  int32_t swapped = err;

  js_value_t *result;
  err = js_create_int32(env, swapped, &result);
  assert(err == 0);

  return result;
}

static int32_t
bare_buffer_typed_swap64(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  uint8_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  bare_buffer__swap64(buf, len);

  return 0;
}

static js_value_t *
bare_buffer_swap64(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  uint8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  if (err == 0) bare_buffer__swap64(buf, len);

  int32_t swapped = err;

  js_value_t *result;
  err = js_create_int32(env, swapped, &result);
  assert(err == 0);

  return result;
}

static int64_t
bare_buffer_typed_index_of(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_value_t *needle_handle,
  int64_t needle_offset,
  int64_t needle_len,
  int64_t from,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  if (!bare_buffer__is_buffer(env, handle)) return BARE_BUFFER_INVALID_BUFFER;
  if (!bare_buffer__is_buffer(env, needle_handle)) return BARE_BUFFER_INVALID_BUFFER;

  uint8_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  uint8_t *needle;
  err = bare_buffer__slice(env, needle_handle, needle_offset, needle_len, (void **) &needle);
  if (err != 0) return err;

  return bare_buffer__index_of(buf, len, needle, needle_len, from);
}

static js_value_t *
bare_buffer_index_of(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 7;
  js_value_t *argv[7];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  if (!bare_buffer__check_buffer(env, argv[3])) return NULL;
  if (!bare_buffer__check_number(env, argv[4], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[5], "Length must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[6], "Start must be a number")) return NULL;

  int64_t needle_offset = bare_buffer__to_int64(env, argv[4]);
  int64_t needle_len = bare_buffer__to_int64(env, argv[5]);
  int64_t from = bare_buffer__to_int64(env, argv[6]);

  uint8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  int64_t at = err;

  if (err == 0) {
    uint8_t *needle;
    err = bare_buffer__slice(env, argv[3], needle_offset, needle_len, (void **) &needle);

    at = err == 0 ? bare_buffer__index_of(buf, len, needle, needle_len, from) : err;
  }

  js_value_t *result;
  err = js_create_int64(env, at, &result);
  assert(err == 0);

  return result;
}

static int64_t
bare_buffer_typed_last_index_of(
  js_value_t *receiver,
  js_value_t *handle,
  int64_t offset,
  int64_t len,
  js_value_t *needle_handle,
  int64_t needle_offset,
  int64_t needle_len,
  int64_t from,
  js_typed_callback_info_t *info
) {
  int err;

  js_env_t *env;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  if (!bare_buffer__is_buffer(env, handle)) return BARE_BUFFER_INVALID_BUFFER;
  if (!bare_buffer__is_buffer(env, needle_handle)) return BARE_BUFFER_INVALID_BUFFER;

  uint8_t *buf;
  err = bare_buffer__slice(env, handle, offset, len, (void **) &buf);
  if (err != 0) return err;

  uint8_t *needle;
  err = bare_buffer__slice(env, needle_handle, needle_offset, needle_len, (void **) &needle);
  if (err != 0) return err;

  return bare_buffer__last_index_of(buf, len, needle, needle_len, from);
}

static js_value_t *
bare_buffer_last_index_of(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 7;
  js_value_t *argv[7];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  if (!bare_buffer__check_buffer(env, argv[0])) return NULL;
  if (!bare_buffer__check_number(env, argv[1], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[2], "Length must be a number")) return NULL;

  int64_t offset = bare_buffer__to_int64(env, argv[1]);
  int64_t len = bare_buffer__to_int64(env, argv[2]);

  if (!bare_buffer__check_buffer(env, argv[3])) return NULL;
  if (!bare_buffer__check_number(env, argv[4], "Offset must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[5], "Length must be a number")) return NULL;
  if (!bare_buffer__check_number(env, argv[6], "Start must be a number")) return NULL;

  int64_t needle_offset = bare_buffer__to_int64(env, argv[4]);
  int64_t needle_len = bare_buffer__to_int64(env, argv[5]);
  int64_t from = bare_buffer__to_int64(env, argv[6]);

  uint8_t *buf;
  err = bare_buffer__slice(env, argv[0], offset, len, (void **) &buf);

  int64_t at = err;

  if (err == 0) {
    uint8_t *needle;
    err = bare_buffer__slice(env, argv[3], needle_offset, needle_len, (void **) &needle);

    at = err == 0 ? bare_buffer__last_index_of(buf, len, needle, needle_len, from) : err;
  }

  js_value_t *result;
  err = js_create_int64(env, at, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_buffer_exports(js_env_t *env, js_value_t *exports) {
  int err;

  js_value_t *constants;
  err = js_create_object(env, &constants);
  assert(err == 0);

  err = js_set_named_property(env, exports, "constants", constants);
  assert(err == 0);

  bare_buffer__read_limits(env);

#define V(name, value) \
  { \
    js_value_t *val; \
    err = js_create_int64(env, value, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, constants, name, val); \
    assert(err == 0); \
  }

  V("MAX_LENGTH", bare_buffer__arraybuffer_limit());
  V("MAX_STRING_LENGTH", bare_buffer__string_limit());
#undef V

#define V(name, untyped, signature, typed) \
  { \
    js_value_t *val; \
    if (signature) { \
      err = js_create_typed_function(env, name, -1, untyped, signature, typed, NULL, &val); \
      assert(err == 0); \
    } else { \
      err = js_create_function(env, name, -1, untyped, NULL, &val); \
      assert(err == 0); \
    } \
    err = js_set_named_property(env, exports, name, val); \
    assert(err == 0); \
  }

  V("alloc", bare_buffer_alloc, NULL, NULL);

  V("allocUnsafe", bare_buffer_alloc_unsafe, NULL, NULL);

  V(
    "byteLengthUTF8",
    bare_buffer_byte_length_utf8,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int64,
      .args_len = 2,
      .args = (int[]){
        js_object,
        js_string,
      }
    }),
    bare_buffer_typed_byte_length_utf8
  );

  V("toStringUTF8", bare_buffer_to_string_utf8, NULL, NULL);

  V(
    "validateUTF8",
    bare_buffer_validate_utf8,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int32,
      .args_len = 4,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
      }
    }),
    bare_buffer_typed_validate_utf8
  );

  V(
    "writeUTF8",
    bare_buffer_write_utf8,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int64,
      .args_len = 5,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
        js_string,
      }
    }),
    bare_buffer_typed_write_utf8
  );

  V("toStringUTF16LE", bare_buffer_to_string_utf16le, NULL, NULL);

  V(
    "writeUTF16LE",
    bare_buffer_write_utf16le,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int64,
      .args_len = 5,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
        js_string,
      }
    }),
    bare_buffer_typed_write_utf16le
  );

  V("toStringLatin1", bare_buffer_to_string_latin1, NULL, NULL);

  V(
    "writeLatin1",
    bare_buffer_write_latin1,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int64,
      .args_len = 5,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
        js_string,
      }
    }),
    bare_buffer_typed_write_latin1
  );

  V("toStringBase64", bare_buffer_to_string_base64, NULL, NULL);
  V("toStringBase64URL", bare_buffer_to_string_base64url, NULL, NULL);

  V(
    "writeBase64",
    bare_buffer_write_base64,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int64,
      .args_len = 5,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
        js_string,
      }
    }),
    bare_buffer_typed_write_base64
  );

  V("toStringHex", bare_buffer_to_string_hex, NULL, NULL);

  V(
    "writeHex",
    bare_buffer_write_hex,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int64,
      .args_len = 5,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
        js_string,
      }
    }),
    bare_buffer_typed_write_hex
  );

  V(
    "validateAscii",
    bare_buffer_validate_ascii,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int32,
      .args_len = 4,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
      }
    }),
    bare_buffer_typed_validate_ascii
  );

  V(
    "swap16",
    bare_buffer_swap16,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int32,
      .args_len = 4,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
      }
    }),
    bare_buffer_typed_swap16
  );

  V(
    "swap32",
    bare_buffer_swap32,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int32,
      .args_len = 4,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
      }
    }),
    bare_buffer_typed_swap32
  );

  V(
    "swap64",
    bare_buffer_swap64,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int32,
      .args_len = 4,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
      }
    }),
    bare_buffer_typed_swap64
  );

  V(
    "indexOf",
    bare_buffer_index_of,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int64,
      .args_len = 8,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
        js_object,
        js_int64,
        js_int64,
        js_int64,
      }
    }),
    bare_buffer_typed_index_of
  );

  V(
    "lastIndexOf",
    bare_buffer_last_index_of,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int64,
      .args_len = 8,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
        js_object,
        js_int64,
        js_int64,
        js_int64,
      }
    }),
    bare_buffer_typed_last_index_of
  );

  V(
    "compare",
    bare_buffer_compare,
    &((js_callback_signature_t){
      .version = 0,
      .result = js_int32,
      .args_len = 7,
      .args = (int[]){
        js_object,
        js_object,
        js_int64,
        js_int64,
        js_object,
        js_int64,
        js_int64,
      }
    }),
    bare_buffer_typed_compare
  );
#undef V

  return exports;
}

BARE_MODULE(bare_buffer, bare_buffer_exports)
