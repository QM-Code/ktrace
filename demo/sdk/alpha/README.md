# Alpha Demo SDK

Exists for CI and as a minimal reference for integrating an SDK add-on with KtraceSDK.

This SDK demonstrates the library-side pattern:
- expose `GetTraceLogger()`
- build a shared `ktrace::TraceLogger("alpha")` with local channels
- emit with `GetTraceLogger().trace(...)` and `GetTraceLogger().info/warn/error(...)`
