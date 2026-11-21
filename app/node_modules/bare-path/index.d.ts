/** Path manipulation utilities, mirroring Node.js's `path` module. */
declare namespace path {
  /** The platform-specific path segment separator: `\` on Windows, `/` elsewhere. */
  export const sep: '/' | '\\'
  /** The platform-specific path delimiter: `;` on Windows, `:` elsewhere. */
  export const delimiter: ':' | ';'

  /**
   * Returns the last portion of `path`, similar to the Unix `basename` command.
   * @param path - The path to extract the last portion from.
   * @param suffix - An optional suffix to remove from the end of the result, if it matches exactly.
   * @returns An empty string if `suffix` equals `path` exactly.
   * @throws {TypeError} thrown if `path`, or a defined `suffix`, is not a string.
   */
  export function basename(path: string, suffix?: string): string

  /**
   * Returns the directory name of `path`, similar to the Unix `dirname` command.
   * @param path - The path to extract the directory name from.
   * @throws {TypeError} thrown if `path` is not a string.
   */
  export function dirname(path: string): string

  /**
   * Returns the extension of `path`, from the last `.` to the end of the string.
   * @param path - The path to extract the extension from.
   * @returns An empty string if `path` has no `.` in its last segment, or if that segment's only
   * `.` is its first character (for example a dotfile).
   * @throws {TypeError} thrown if `path` is not a string.
   */
  export function extname(path: string): string

  /**
   * Returns `true` if `path` is an absolute path.
   * @param path - The path to test.
   * @returns On Windows, a drive-qualified path such as `C:\foo` is also absolute; on POSIX, only
   * paths starting with `/` are absolute.
   * @throws {TypeError} thrown if `path` is not a string.
   */
  export function isAbsolute(path: string): boolean

  /**
   * Joins all given path segments using the platform-specific separator, then normalizes the
   * result.
   * @param paths - The path segments to join together, in the order given.
   * @throws {TypeError} thrown if any path segment is not a string.
   */
  export function join(...paths: string[]): string

  /**
   * Normalizes `path`, resolving `.` and `..` segments.
   * @param path - The path to normalize.
   * @throws {TypeError} thrown if `path` is not a string.
   */
  export function normalize(path: string): string

  /**
   * Returns the relative path from `from` to `to`.
   * @param from - The path to resolve the relative path from.
   * @param to - The path to resolve the relative path to.
   * @returns Both `from` and `to` are resolved to absolute paths before comparing, and an empty
   * string is returned if they resolve to the same path.
   * @throws {TypeError} thrown if `from` or `to` is not a string.
   */
  export function relative(from: string, to: string): string

  /**
   * Resolves a sequence of paths or path segments into an absolute path.
   * @param args - The path segments to resolve, processed from right to left until an absolute path
   * is constructed.
   * @returns Falls back to the current working directory (or that of the resolved drive, on
   * Windows) if none of the given segments is absolute.
   * @throws {TypeError} thrown if any path segment is not a string.
   */
  export function resolve(...args: string[]): string

  /**
   * Converts `path` to a namespace-prefixed path on Windows; returns `path` unchanged on POSIX.
   * @param path - The path to convert.
   * @returns On Windows, resolves `path` and prefixes UNC paths with `\\?\UNC\` and drive-letter
   * paths with `\\?\`; other paths (including non-string input) are returned unchanged.
   */
  export function toNamespacedPath(path: string): string

  /** The POSIX-specific implementation of the `path` methods. */
  export const posix = path
  /** The Windows-specific implementation of the `path` methods. */
  export const win32 = path
}

export = path
