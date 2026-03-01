# m-ktrace demos

- `initialize/` minimal direct trace demo.
- `libraries/alpha/` produces `AlphaSDK` with `alpha::InitializeTraceLogging()` and `alpha::TestTraceLoggingChannels()`.
- `libraries/beta/` produces `BetaSDK` with `beta::InitializeTraceLogging()` and `beta::TestTraceLoggingChannels()`.
- `libraries/delta/` produces `DeltaSDK` with `delta::InitializeTraceLogging()` and `delta::TestTraceLoggingChannels()`.
- `executable/` consumes KTraceSDK + AlphaSDK + BetaSDK + DeltaSDK.
