# Core Demo

Basic local-plus-imported tracing showcase for KtraceSDK and the alpha demo SDK.

This demo shows:
- executable-local tracing defined with a local `ktrace::TraceLogger`
- imported SDK tracing added via `alpha::GetTraceLogger()`
- logger-managed selector state and output formatting
- local CLI integration through `parser.addInlineParser(logger.makeInlineParser(local_trace_logger))`
