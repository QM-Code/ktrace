# m-ktrace

Standalone trace logging library extracted as its own SDK module.

`KTRACE_NAMESPACE` must be defined by consumers before using
`ktrace/trace.hpp` (for example via target compile definitions).

## Build

```bash
./abuild.py -a <name> -d build/test/
```

The default SDK install output is:

- `build/test/sdk/include`
- `build/test/sdk/lib`
- `build/test/sdk/lib/cmake/KTraceSDK`

You can override the install prefix with `-k` / `--install-sdk`.
