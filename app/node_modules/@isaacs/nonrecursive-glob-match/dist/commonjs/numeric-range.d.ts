import type { Item } from './index.js';
/**
 * The strings that the bash brace expansion `{lo..hi}` makes, as
 * the alternatives of an `@(...)` extglob. Each one is a sequence of
 * digits and digit classes, so there are only a few of them, even for
 * a large range.
 *
 * If either end has a leading zero, such as `{01..10}`, each number
 * has leading zeros to make it as long as the longer end, counting a
 * `-` sign. So `{-05..3}` makes `-05` to `-01`, then `000` to `003`.
 *
 * Returns undefined if either end is not a safe integer.
 */
export declare const numericRange: (loStr: string, hiStr: string) => Item[][] | undefined;
//# sourceMappingURL=numeric-range.d.ts.map