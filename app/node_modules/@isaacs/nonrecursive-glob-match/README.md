# @isaacs/nonrecursive-glob-match

This is a glob matcher that can be applied to a single path
portion (ie, not `/`), which completes in polynomial time because
it doesn't compile to Regular Expressions or any other
backtrack-dependent algorithm.

## USAGE

```ts
import { Matcher } from '@isaacs/nonrecursive-glob-match'

const m = new Matcher('a*a!(y|z)')

console.log(m.test('abcay')) // false
console.log(m.test('abcax')) // true
console.log(m.test('aa')) // true
```

The pattern and the tested string must be a single path portion.
If either one contains a `/` character, the matcher throws an
error.

If you already split a path on `/`, use `m.testUnchecked(s)`. It
gives the same result as `m.test(s)`, but it does not check `s`
for a `/` character. This can make it faster, because some
patterns, such as `*.js`, then do not need to look at each
character. If `s` has a `/`, the result is not defined.

## Why

Patterns like this frustrate most glob matchers: `a*a*a*a*a*a*a`.
They can be used to take down servers if applied to untrusted
input. Fixing these vulns is like playing whack-a-mole with the
whole internet.

If compiled to a regular expression like
`/^a[^/]*a[^/]*a[^/]*a[^/]*a[^/]*a[^/]*a$/`, and then applied to
a path portion like `aaaaaaaaaaaaaaaaaaab`, the engine must check
every possible path through the pattern, constantly rewinding
back to the first character each time and trying a different
number of `a` characters matched against the `[^/]*` to see if it
matches. Because nothing matches, it must test every possible
path before ultimately giving up.

In contrast, the algorithm presented here is based on the
Thompson NFA paper from 1968, which tests every possible path
through the string at the same time, so that it only has to walk
the string one time. This algorithm is also implemented in the
RegExp engines used by `egrep` and `awk`, which is why those
tools are so fast, even when matching complicated RegExps against
extremely large strings.

The `Matcher` objects have a `test` method which takes a string,
just like a `RegExp` object, and can be a drop-in replacement for
the regular expressions used in minimatch and other matchers.
However, they are _not_ a complete drop-in replacement for a
RegExp, so be careful when replacing one with the other. (For
example, they do not capture, do not track match indexes, no
fancy RegExp features, etc.)

## Patterns Supported

Here are the patterns supported, and their effective state
machines drawn in lovely ASCII diagrams.

### `*` 0 or more of any character, not starting with `.`

```
--+-------------------+->
  v                   ^
  +<------------------+
  v                   ^
  +-->any character-->+
```

### `?`

```
-->any character-->
```

### `[<set>]`

```
-->character in set-->
```

This covers named posix sets like `[[:alpha:]]` as well as
character classes like `[a-b]`. (In this case, a regular
expression actually is compiled, but it's not subject to ReDoS,
becuase it's just a `/[a-z]/` style pattern tested against one
character at a time.)

### `@(e1|e2|...)`

"One" extglob.

```
--+--> e1 -->+-->
  v          ^
  +--> e2 -->+
  v          ^
  +--> e3 -->+
  v   ...    ^
```

### `?(e1|e2|...)`

"Zero or one" extglob.

```
--+--------->+-->
  v          ^
  +--> e1 -->+
  v          ^
  +--> e2 -->+
  v          ^
  +--> e3 -->+
  v   ...    ^
```

### `*(e1|e2|...)`

"Zero or more" extglob.

```
--+--------->+-->
  v          ^
  +<---------+
  v          ^
  +--> e1 -->+
  v          ^
  +--> e2 -->+
  v          ^
  +--> e3 -->+
  v   ...    ^
```

### `+(e1|e2|...)`

"One or more" extglob.

```
  +<---------+
  v          ^
--+--> e1 -->+-->
  v          ^
  +--> e2 -->+
  v          ^
  +--> e3 -->+
  v   ...    ^
```

### `!(e1|e2|...)`

"Negative" extglob.

The challenge with negation is that it opens the door to a whole
heap of edge cases. Especially, _any_ expression can be nested
inside the negation, and it needs to match anything at all, as
long as it _doesn't_ match one of the supplied patterns. This is
where it gets complicated, and past attempts have run into
problems.

We handle this by starting a "Run" that technically does recurse,
and thus can push the performance out from `O(n * m)` to
`O(n * m * r)` where `n` is the string length, `m` is the pattern
length, and `r` is the number of negative extglob portions it
contains. In practice, this is limited by the pattern length, and
still doesn't allow ReDOS-style explosive recursive backtracking,
so this is fine.

## Unicode Normalization

A char such as `é` can be one code point (U+00E9), or `e`
followed by a combining accent (U+0301). Some file systems, such
as on macOS, can give file names in the second form. These two
forms do not match by default.

Set the `normalize` option to normalize the pattern and each
tested string to the same form. Use `'NFC'`, `'NFD'`, `'NFKC'`,
or `'NFKD'`. `true` means `'NFC'`.

```ts
const m = new Matcher('café', { normalize: true })
m.test('café') // true
```

- Use NFC if you are not sure. With NFD and NFKD, `é` is two
  code points. Then `?` does not match `é`, and `[é]` can not
  match it.
- NFKC and NFKD also fold compatibility chars. For example, `ﬁ`
  matches `fi`, and full-width `Ａ` matches `A`.
- Syntax is parsed before normalization. So, a char that
  normalizes to a syntax char stays a literal. For example, with
  NFKC, full-width `＊` matches only `*` or `＊`, and it is not a
  wildcard.
- Each run of literal chars normalizes as one string. Each member
  of a `[...]` class normalizes on its own, and stays as it is if
  its normal form is more than one code point.
- A combining mark after a wildcard is its own literal, and does
  not compose with the char that the wildcard matches. So, with
  NFC, `?́` does not match `é`. With NFD, it does match,
  because `é` is also two code points in the string. Patterns
  match code points, not grapheme clusters.
- With `nocase`, normalization comes first, then case folding.
- The `/` check uses the strings as given. So, full-width `／`
  does not cause an error.

## Numeric Ranges

Bash brace expansion makes `{1..1000}` into 1000 words. A glob
library that expands braces before matching, such as
[minimatch](http://npm.im/minimatch), can use the `rangeMark`
option to match a range in one pattern instead.

Set `rangeMark` to a char that is not otherwise in the pattern.
Then `<mark><lo>..<hi><mark>` matches the same strings as the
brace expansion `{<lo>..<hi>}`, where `lo` and `hi` are integers.
If either end has a leading zero, each number has leading zeros to
make it as long as the longer end, counting a `-` sign.

```ts
const M = '\uE000'
const m = new Matcher(`file-${M}001..500${M}.txt`, {
  rangeMark: M,
})
m.test('file-042.txt') // true
m.test('file-42.txt') // false
m.test('file-501.txt') // false
```

The range is matched with a few digit classes, so a large range
costs about the same as a small one.

- A `\` before the mark does not escape it.
- Inside a `[...]` class, the mark is a char of the class.
- A mark without a valid range after it, or without a closing
  mark, is a literal char.

## Differences from Bash

Bash 5 is the reference for this module, with two known
deviations.

### Dotfiles

If `dot` is not set, a string that starts with `.` only matches
if a literal `.` in the pattern matches that first character.
Extglobs before that `.` can match the empty string, and the
pattern can still match. For example, `!(.a|js)@(.*)` matches
`.a`, `.a.js`, and `.js`, because `!(.a|js)` matches the empty
string, and `@(.*)` matches the rest. Bash does not match these
dotfiles. See [this bug
report](https://lists.gnu.org/archive/html/bug-bash/2026-03/msg00034.html).

A `*` can never match at the start of a dotfile, even if it
matches the empty string. So, `*.js` does not match `.js`, as in
Bash.

### Case-insensitive matching

Bash compares chars with the case functions of the C library for
the current locale. So, the results can change with the locale
and the platform.

With `nocase`, this module uses Unicode simple case folding, one
code point at a time. This is the same folding that a JavaScript
RegExp uses with the `iu` flags. It does not change with the
locale, and literals, `[...]` classes, and ranges all use it.

- `Σ`, `σ`, and final sigma `ς` all match each other.
- The micro sign `µ` matches `μ` and `Μ`. The Kelvin sign `K`
  matches `k`. The Angstrom sign `Å` matches `å`.
- `ẞ` matches `ß`, but `ß` does not match `ss`. A fold never
  changes the number of chars, so ligatures such as `ﬁ` do not
  match `fi`.
- There are no Turkish rules. `I` matches `i`, but dotted `İ`
  and dotless `ı` only match themselves.

## Algorithm

A glob pattern is parsed and compiled into a series of attached
state machines.

For example, the glob patttern `*(a|[c-d]|+(x|y|z))z` would
compile to a state machine something like the following flow
diagram:

```
-->+----------------------->+-->8 LITERAL(z)-->9 end
   v                        ^
   +<-----------------------6
   v                        ^
   0-->LITERAL(a)---------->|
   v                        |
   1-->CLASS(c-d)---------->|
   v                        |
   2-->+<---------------+-->+
       v                ^
       3-->LITERAL(x)-->7
       v                |
       4-->LITERAL(y)-->|
       v                |
       5-->LITERAL(z)-->+
```

Then, for each character in the string, we walk through the
resulting graph until we find a test node.

These test nodes are all executed on the current character, and
for any tests pass, their output nodes are added to the walk, and
we proceed to walk until hitting a test node again.

Because all routes eventually get to the exit, which only passes
when the string is at its last character, we are guaranteed to
eventually either fail (and have no more states to process), or
exit with a match.

At each branching point, effectively we increase the memory usage
by a tiny bit to track which nodes are being processed, but
importantly, we _don't_ have to try all the way to the exit and
then backtrack recursively, storing multiple states of the string
in an exponential explosion.

A pattern like `a*a*a*a*a*a*a*a*a*a*a`, which could be naively
converted into a Regular Expression
`^a.*a.*a.*a.*a.*a.*a.*a.*a.*a.*a$`, would take over 33 seconds
to match the string `'a'.repeat(50) + 'b'` using standard
JavaScript regular expressions, because it must attempt every
possible grouping of characters, backtracking after each failure.

By contrast, this implementation can solve this match for either
a pass or failure in roughly the same amount of time, which
is bound by `C * N`, where `C` is the number of states in the
machine, and `N` is the length of the string, finishing both
failure and success cases in polynomial time.

Even on the first rather naive implementation in JavaScript, with
no optimizations for repeated `*` or other common tweaks, this
finishes in 300-400 µs for the pathological case described, and
as the numbers increase, this time rises roughly linearly.

That's somewhat worse than RegExp's performance in the _success_
case, which is around 40µs, but as it's around 100,000x faster in
the failure case, this is already quite promising. And, future
iterations of this can likely bring that performance closer to
the native RegExp implementation.

## Acknowledgement

The algorithm and ideas in this implementation are inspired,
borrowed, and at times outright stolen from [Russ
Cox](https://research.swtch.com)'s explanation of Ken Thompson's
work in [Regular Expression Matching Can Be Simple And
Fast](https://swtch.com/~rsc/regexp/regexp1.html). I am both
impressed by and grateful for their work.

The code here in this library was written entirely by me, [Isaac
Z. Schlueter](https://izs.me), and so you should not blame either
of them (or anyone else) for any problems it might have. You
should also not blame me. It's open source, after all, no
warrantees express or implied, etc.
