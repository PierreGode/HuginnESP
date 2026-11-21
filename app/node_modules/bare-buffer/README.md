# bare-buffer

Native buffers for JavaScript.

```
npm i bare-buffer
```

## Usage

```js
const Buffer = require('bare-buffer')

const message = Buffer.from('hello')
const empty = Buffer.alloc(16)
const buffer = Buffer.allocUnsafe(4).fill(123)
const combined = Buffer.concat([buffer, buffer])
```

## API

See the [`bare-buffer` reference](https://docs.pears.com/reference/bare/modules/bare-buffer).

## Threat model

`bare-buffer` is one of the addons Bare compiles into its binary, so it inherits [Bare's threat model](https://github.com/holepunchto/bare/blob/main/docs/threat-model.md). See [`docs/threat-model.md`](docs/threat-model.md) for where this addon sits in it.

## License

Apache-2.0
