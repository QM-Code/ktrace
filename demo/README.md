# Karma Trace Logging Demos

## Build in this order

- `initialize/` minimal direct trace demo.
- `libraries/alpha/` produces `AlphaSDK` with `alpha::Init()` and `alpha::TestTraceLoggingChannels()`.
- `libraries/beta/` produces `BetaSDK` with `beta::InitializeTraceLogging()` and `beta::TestTraceLoggingChannels()`.
- `libraries/delta/` produces `DeltaSDK` with `delta::SystemStartup()` and `delta::TestTraceLoggingChannels()`.
- `executable/` consumes KTraceSDK + AlphaSDK + BetaSDK + DeltaSDK.

## Running tests

```bash
./demo/executable/build/latest/test
./demo/executable/build/latest/test --trace
./demo/executable/build/latest/test --trace '*'
./demo/executable/build/latest/test --trace '*.*'
./demo/executable/build/latest/test --trace '*.*.*'
./demo/executable/build/latest/test --trace-namespaces
./demo/executable/build/latest/test --trace-channels
./demo/executable/build/latest/test --trace-colors
...
```

