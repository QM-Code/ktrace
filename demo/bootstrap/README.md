# Bootstrap Demo

Exists for CI and as the smallest compile/link usage reference for KTraceSDK.

This demo shows the minimal executable-side setup:
- create a `ktrace::Logger`
- create a local `ktrace::TraceLogger`
- add one or more channels
- `logger.addTraceLogger(...)`
- `logger.activate()`
- emit with `KTRACE(...)`
