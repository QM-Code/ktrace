# Karma Trace Logging SDK

## Build SDK

```bash
./kbuild.py
```
SDK Output:
- `build/latest/sdk/include`
- `build/latest/sdk/lib`
- `build/latest/sdk/lib/cmake/KTraceSDK`

## Build Demos

```bash
./kbuild.py --build-demos

./demo/executable/build/latest/test
```
Demos:
- Libraries: `demo/libraries/{alpha,beta,delta}`
- Executable: `demo/executable/`

Demo libraries demonistrate how other libraries can implement and expose ktrace.

The demo executable demonstrates how an executable can enable ktrace locally as well as for imported libraries (assuming they were build around ktrace).

## Install

`KTRACE_NAMESPACE` must be defined by consumers before using

Generally this is done in CMakeLists.txt, though it can also be done at source level.

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
- `ktrace::EnableChannels("alpha.*,{beta,delta}.net.*");`
- `ktrace::EnableChannels(".net.*,otherapp.scheduler.tick");`

## Coding Agents

If you are using a coding agent, paste the follwing prompt:

```bash
Follow the instrctions in agent/BOOTSTRAP.md
```
