# Karma Trace Logging SDK

Trace logging SDK with:
- namespaced channel tracing via `TraceLogger::trace(...)`
- always-visible operational logging via `TraceLogger::info/warn/error(...)`
- a library-facing `TraceLogger` source object
- an executable-facing `Logger` registry, filter, formatter, and output sink

## Build SDK

```bash
./kbuild.py --build-latest
```

SDK output:
- `build/latest/sdk/include`
- `build/latest/sdk/lib`
- `build/latest/sdk/lib/cmake/KtraceSDK`

## Build And Test Demos

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

## API Model

`TraceLogger` is the namespace-bearing source object. Construct it with an explicit namespace and declare channels on it:

```cpp
ktrace::TraceLogger trace("alpha");
trace.addChannel("net", ktrace::Color("DeepSkyBlue1"));
trace.addChannel("cache", ktrace::Color("Gold3"));
```

SDKs should usually expose a shared handle from `GetTraceLogger()`:

```cpp
ktrace::TraceLogger GetTraceLogger() {
    static ktrace::TraceLogger trace("alpha");
    static const bool initialized = [] {
        trace.addChannel("net", ktrace::Color("DeepSkyBlue1"));
        trace.addChannel("cache", ktrace::Color("Gold3"));
        return true;
    }();
    (void)initialized;
    return trace;
}
```

Because `TraceLogger` is a shared-state handle, copies returned from `GetTraceLogger()` still refer to the same trace source.

`Logger` is the executable-facing runtime. It imports one or more `TraceLogger`s, maintains the central channel registry, and owns filtering, formatting, and final output:

```cpp
ktrace::Logger logger;

ktrace::TraceLogger app_trace("core");
app_trace.addChannel("app", ktrace::Color("BrightCyan"));
app_trace.addChannel("startup", ktrace::Color("BrightYellow"));

logger.addTraceLogger(app_trace);
logger.addTraceLogger(alpha::GetTraceLogger());
```

## Logging APIs

Channel-based trace output:

```cpp
trace.trace("channel", "message {}", value);
trace.traceChanged("channel", key, "message {}", value);
```

Always-visible operational logging:

```cpp
trace.info("message");
trace.warn("configuration file '{}' was not found", path);
trace.error("fatal startup failure");
```

Operational logging is independent of channel enablement. It is still namespaced and uses the same formatting options as trace output.

Message formatting supports sequential `{}` placeholders and escaped braces `{{` and `}}`.

## CLI Integration

The inline parser is now logger-bound rather than global. Pass the executable's local `TraceLogger` so leading-dot selectors resolve against the right namespace:

```cpp
ktrace::Logger logger;
ktrace::TraceLogger app_trace("core");
app_trace.addChannel("app", ktrace::Color("BrightCyan"));

logger.addTraceLogger(app_trace);

kcli::Parser parser;
parser.addInlineParser(logger.makeInlineParser(app_trace));

parser.parseOrExit(argc, argv);
```

## Channel Expression Forms

Single-selector APIs on `ktrace::Logger`:
- `.channel[.sub[.sub]]` for a local channel in the provided local namespace
- `namespace.channel[.sub[.sub]]` for an explicit namespace

List APIs on `ktrace::Logger`:
- `enableChannels(...)`
- `disableChannels(...)`
- list APIs accept selector patterns such as `*`, `{}`, and CSV
- list APIs resolve selectors against the channels currently registered at call time
- leading-dot selectors in list APIs resolve against the provided local namespace
- empty/whitespace selector lists are rejected
- unregistered channels remain disabled and do not emit, even if a selector pattern would otherwise match

Examples:
- `logger.enableChannel(app_trace, ".app");`
- `logger.enableChannel("alpha.net");`
- `logger.enableChannels("alpha.*,{beta,gamma}.net.*");`
- `logger.enableChannels(app_trace, ".net.*,otherapp.scheduler.tick");`

Formatting options:
- `--trace-files`
- `--trace-functions`
- `--trace-timestamps`

These affect both `trace(...)` output and `info/warn/error(...)` output.

## Coding Agents

If you are using a coding agent, paste the following prompt:

```bash
Follow the instructions in agent/BOOTSTRAP.md
```
