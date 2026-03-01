# m-ktrace demos

- `initialize/` minimal direct trace demo.
- `libraries/alpha/` produces `AlphaSDK` with `alpha::Init()` and `alpha::TestTraceLoggingChannels()`.
- `libraries/beta/` produces `BetaSDK` with `beta::InitializeTraceLogging()` and `beta::TestTraceLoggingChannels()`.
- `libraries/delta/` produces `DeltaSDK` with `delta::SystemStartup()` and `delta::TestTraceLoggingChannels()`.
- `executable/` consumes KTraceSDK + AlphaSDK + BetaSDK + DeltaSDK.
