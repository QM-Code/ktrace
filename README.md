# Karma Trace Logging SDK

Trace logging library.

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

Build the parser up front, add the ktrace inline parser, and wrap the single
`parse(argc, argv)` call in a `try`/`catch` block:

```cpp
kcli::PrimaryParser parser;
parser.addInlineParser(ktrace::GetInlineParser("trace"));

try {
    parser.parse(argc, argv);
} catch (const kcli::CliError& ex) {
    std::cerr << "CLI error: " << ex.what() << "\n";
    return 2;
}
```

## Install

`KTRACE_NAMESPACE` must be defined by consumers before use.

This is generally done in `CMakeLists.txt`, though it can also be done at source level.

Always-visible operational logging:
- `ktrace::log::Info("message")`
- `ktrace::log::Warn("message")`
- `ktrace::log::Error("message")`
- `ktrace::log::Warn("configuration file '{}' was not found", path)`
- these are independent of trace-channel enablement
- they use the same namespace/timestamp/source formatting options as trace output

## Channel Expression Forms

Single-selector APIs:
- `.channel[.sub[.sub]]` for a local channel in the current `KTRACE_NAMESPACE`
- `namespace.channel[.sub[.sub]]` for an explicit namespace

List APIs:
- `EnableChannels(...)`
- `DisableChannels(...)`
- list APIs accept selector patterns such as `*`, `{}`, and CSV
- list APIs resolve selectors against the channels currently registered at call time
- leading-dot selectors in list APIs resolve against current `KTRACE_NAMESPACE`
- empty/whitespace selector lists are rejected
- unregistered channels remain disabled and do not emit, even if a selector pattern would otherwise match

Examples:
- `ktrace::EnableChannel(".abc");`
- `ktrace::EnableChannel(".abc.xyz");`
- `ktrace::EnableChannel("otherapp.channel");`
- `ktrace::EnableChannels("alpha.*,{beta,gamma}.net.*");`
- `ktrace::EnableChannels(".net.*,otherapp.scheduler.tick");`

## Coding Agents

If you are using a coding agent, paste the following prompt:

```bash
Follow the instructions in agent/BOOTSTRAP.md
```
