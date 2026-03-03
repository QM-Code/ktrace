# Karma Trace Logging SDK

## Build SDK

```bash
./kbuild.py
```
SDK output:
- `build/latest/sdk/include`
- `build/latest/sdk/lib`
- `build/latest/sdk/lib/cmake/KTraceSDK`

## Build and Test Demos

```bash
# Uses kbuild.json "build-demos" order.
./kbuild.py --build-demos

# Or provide explicit order.
./kbuild.py --build-demos libraries/alpha libraries/beta libraries/gamma executable

./demo/executable/build/latest/test
```
Demos:
- Libraries: `demo/libraries/{alpha,beta,gamma}`
- Executable: `demo/executable/`

Demo builds are orchestrated by the root `kbuild.py`

Demo libraries demonstrate how other libraries can implement and expose ktrace.

The demo executable shows how an executable can enable ktrace locally and for imported libraries (assuming they were built around ktrace).

Trace CLI examples:

```bash
./demo/executable/build/latest/test --trace
./demo/executable/build/latest/test --trace '.*'
./demo/executable/build/latest/test --trace '*.*'
./demo/executable/build/latest/test --trace '*.*.*'
./demo/executable/build/latest/test --trace '*.{net,io}'
./demo/executable/build/latest/test --trace-namespaces
./demo/executable/build/latest/test --trace-channels
./demo/executable/build/latest/test --trace-colors
```

## Install

`KTRACE_NAMESPACE` must be defined by consumers before use.

This is generally done in `CMakeLists.txt`, though it can also be done at source level.

## Channel Expression Forms

Single-selector APIs:
- `.channel[.sub[.sub]]` for a local channel in the current `KTRACE_NAMESPACE`
- `namespace.channel[.sub[.sub]]` for an explicit namespace

List APIs:
- `EnableChannels(...)`
- `DisableChannels(...)`
- list APIs accept selector patterns such as `*`, `{}`, and CSV
- leading-dot selectors in list APIs resolve against current `KTRACE_NAMESPACE`

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
