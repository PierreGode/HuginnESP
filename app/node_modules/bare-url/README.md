# bare-url

WHATWG URL implementation for JavaScript, built on <https://github.com/holepunchto/liburl>. Provides `URL` and `URLSearchParams` classes compatible with the WHATWG URL Standard.

```
npm i bare-url
```

## Usage

```js
const { URL, URLSearchParams } = require('bare-url')

const url = new URL('https://example.com/path?foo=bar#hash')

console.log(url.hostname) // 'example.com'
console.log(url.pathname) // '/path'
console.log(url.searchParams.get('foo')) // 'bar'
```

To register `URL` and `URLSearchParams` as globals:

```js
require('bare-url/global')
```

## API

See the [`bare-url` reference](https://docs.pears.com/reference/bare/modules/bare-url).

## Threat model

`bare-url` is one of the addons Bare compiles into its binary, so it inherits [Bare's threat model](https://github.com/holepunchto/bare/blob/main/docs/threat-model.md). See [`docs/threat-model.md`](docs/threat-model.md) for where this addon sits in it.

## License

Apache-2.0
