# Karma Trace Logging SDK

Trace logging SDK with:
- channel-based trace output via `KTRACE(...)`
- always-visible operational logging via `ktrace::Info/Warn/Error(...)`
- a library-facing `TraceLogger` definition object
- an executable-facing `Logger` runtime/orchestrator

## Build SDK

```bash
./kbuild.py --build-latest
```

SDK output:
- `build/latest/sdk/include`
- `build/latest/sdk/lib`
- `build/latest/sdk/lib/cmake/KTraceSDK`

## Build and Test Demos

```bash
# Builds SDK plus kbuild.json "build.defaults.demos".
./kbuild.py --build-latest

# Explicit demo-only run (uses build.demos when no args are provided).
./kbuild.py --build-demos

./demo/exe/core/build/latest/test
```

Demos:
- Bootstrap compile/link check: `demo/bootstrap/`
- SDKs: `demo/sdk/{alpha,beta,gamma}`
- Executables: `demo/exe/{core,omega}`

Demo builds are orchestrated by the root `kbuild.py`.

Demo libraries demonstrate how other libraries can implement and expose ktrace.

The core demo shows local tracing plus imported SDK integration. The omega demo adds the fuller selector and output-format coverage.

Trace CLI examples:

```bash
./demo/exe/core/build/latest/test --trace
./demo/exe/core/build/latest/test --trace '.*'
./demo/exe/omega/build/latest/test --trace '*.*'
./demo/exe/omega/build/latest/test --trace '*.*.*.*'
./demo/exe/omega/build/latest/test --trace '*.{net,io}'
./demo/exe/omega/build/latest/test --trace-namespaces
./demo/exe/omega/build/latest/test --trace-channels
./demo/exe/omega/build/latest/test --trace-colors
```

## CLI Integration

Activate a `ktrace::Logger`, add the global ktrace inline parser, and wrap the
single `parse(argc, argv)` call in a `try`/`catch` block:

```cpp
ktrace::Logger logger;
logger.activate();

kcli::PrimaryParser parser;
parser.addInlineParser(ktrace::GetInlineParser());

try {
    parser.parse(argc, argv);
} catch (const kcli::CliError& ex) {
    std::cerr << "CLI error: " << ex.what() << "\n";
    return 2;
}
```

## Install

`KTRACE_NAMESPACE` must be defined by any target that emits ktrace logging.

This is generally done in `CMakeLists.txt`:

```cmake
target_compile_definitions(my_target PRIVATE KTRACE_NAMESPACE="myapp")
```

`TraceLogger` depends on `KTRACE_NAMESPACE` and automatically binds to it. The
trace macros and `ktrace::Info/Warn/Error(...)` also emit under that namespace.

## API Model

`TraceLogger` is the library-facing definition object. SDKs expose trace support
by returning one from a helper such as `GetTraceLogger()`:

```cpp
ktrace::TraceLogger GetTraceLogger() {
    ktrace::TraceLogger logger;
    logger.addChannel("net", ktrace::Color("DeepSkyBlue1"));
    logger.addChannel("cache", ktrace::Color("Gold3"));
    return logger;
}
```

Libraries that expose a `GetTraceLogger()`-style API do not need to include
`ktrace.hpp` from their own public headers. A public header can forward-declare
`ktrace::TraceLogger` instead. That keeps non-tracing consumers from inheriting
the `KTRACE_NAMESPACE` requirement just by including the library header.

The tradeoff is that any translation unit that actually materializes a
`ktrace::TraceLogger` from that API must include `ktrace.hpp` itself.

`Logger` is the executable-facing runtime. Executables compose one or more
`TraceLogger`s into a single active process logger:

```cpp
ktrace::Logger logger;

ktrace::TraceLogger tracer;
tracer.addChannel("app", ktrace::Color("BrightCyan"));
tracer.addChannel("startup", ktrace::Color("BrightYellow"));

logger.addTraceLogger(tracer);
logger.addTraceLogger(alpha::GetTraceLogger());
logger.activate();
```

After activation:
- `KTRACE(...)` and `KTRACE_CHANGED(...)` emit through the active `Logger`
- `ktrace::Info/Warn/Error(...)` emit through the active `Logger`
- `ktrace::GetInlineParser()` applies CLI changes to the active `Logger`

## Logging APIs

Channel-based trace output:
- `KTRACE("channel", "message {}", value)`
- `KTRACE_CHANGED("channel", key, "message")`

Always-visible operational logging:
- `ktrace::Info("message")`
- `ktrace::Warn("configuration file '{}' was not found", path)`
- `ktrace::Error("fatal startup failure")`

Operational logging is independent of channel enablement. It is still
namespaced and uses the same formatting options as trace output.

## Channel Expression Forms

Single-selector APIs on `ktrace::Logger`:
- `.channel[.sub[.sub]]` for a local channel in the current `KTRACE_NAMESPACE`
- `namespace.channel[.sub[.sub]]` for an explicit namespace

List APIs on `ktrace::Logger`:
- `enableChannels(...)`
- `disableChannels(...)`
- list APIs accept selector patterns such as `*`, `{}`, and CSV
- list APIs resolve selectors against the channels currently registered at call time
- leading-dot selectors in list APIs resolve against current `KTRACE_NAMESPACE`
- empty/whitespace selector lists are rejected
- unregistered channels remain disabled and do not emit, even if a selector pattern would otherwise match

Examples:
- `logger.enableChannel(".abc");`
- `logger.enableChannel(".abc.xyz");`
- `logger.enableChannel("otherapp.channel");`
- `logger.enableChannels("alpha.*,{beta,gamma}.net.*");`
- `logger.enableChannels(".net.*,otherapp.scheduler.tick");`

Formatting options:
- `--trace-files`
- `--trace-functions`
- `--trace-timestamps`

These affect both `KTRACE(...)` output and `ktrace::Info/Warn/Error(...)`.

## Coding Agents

If you are using a coding agent, paste the following prompt:

```bash
Follow the instructions in agent/BOOTSTRAP.md
```
