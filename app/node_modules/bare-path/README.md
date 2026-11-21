# bare-path

Path manipulation library for JavaScript.

```
npm i bare-path
```

## Usage

```js
const path = require('bare-path')

path.join('foo', 'bar') // foo/bar on posix, foo\bar on windows
```

## API

See the [`bare-path` reference](https://docs.pears.com/reference/bare/modules/bare-path).

## Threat model

`bare-path` is one of the addons Bare compiles into its binary, so it inherits [Bare's threat model](https://github.com/holepunchto/bare/blob/main/docs/threat-model.md). See [`docs/threat-model.md`](docs/threat-model.md) for where this addon sits in it.

## License

Apache-2.0
