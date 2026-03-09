# Alpha Demo SDK

Exists for CI and as a minimal reference for integrating an SDK add-on with KTraceSDK.

This SDK demonstrates the library-side pattern:
- define `KTRACE_NAMESPACE` on the target
- expose `GetTraceLogger()`
- build a `ktrace::TraceLogger` with local channels
- emit with `KTRACE(...)` in normal library code
