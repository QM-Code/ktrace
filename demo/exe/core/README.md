# Core Demo

Basic local-plus-imported tracing showcase for KTraceSDK and the alpha demo SDK.

This demo shows:
- executable-local tracing defined with a local `ktrace::TraceLogger`
- imported SDK tracing added via `alpha::GetTraceLogger()`
- `ktrace::Logger` activation before CLI parsing and trace emission
- global CLI integration through `parser.addInlineParser(ktrace::GetInlineParser())`
