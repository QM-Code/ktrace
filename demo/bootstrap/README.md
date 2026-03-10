# Bootstrap Demo

Exists for CI and as the smallest compile/link usage reference for KtraceSDK.

This demo shows the minimal executable-side setup:
- create a `ktrace::Logger`
- create a local `ktrace::TraceLogger("bootstrap")`
- add one or more channels
- `logger.addTraceLogger(...)`
- enable local selectors through `logger.enableChannel(trace_logger, ".channel")`
- emit with `trace_logger.trace(...)`
