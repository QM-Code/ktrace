# Executable Demo

Build executable against KTraceSDK + AlphaSDK + BetaSDK + GammaSDK:

Run from the repository root directory:

```bash
./kbuild.py --build-demos libraries/alpha libraries/beta libraries/gamma executable
```

Run:

```bash
./demo/executable/build/latest/test
```

## CLI Behavior Tests

The executable demo defines CLI behavior checks in:

- `tests/trace_cli_cases.sh`

When `BUILD_TESTING=ON`, `CMakeLists.txt` registers these CTest tests:

1. `demo_cli_unknown_trace_option_shows_help`
   - Purpose: ensure unknown `--trace-*` options show help output.
   - Command under test: `./test --trace-f`
   - Required output:
     - `Trace option error: unknown trace option '--trace-f'`
     - `Trace logging options:`
   - Must not include:
     - `Trace selector examples:`

2. `demo_cli_blank_trace_shows_help`
   - Purpose: ensure blank `--trace` prints help.
   - Command under test: `./test --trace`
   - Required output:
     - `Trace logging options:`
   - Must not include:
     - `Trace option error:`
     - `Trace selector examples:`

3. `demo_cli_bad_selector_shows_examples`
   - Purpose: ensure invalid selectors show examples (not help).
   - Command under test: `./test --trace '*'`
   - Required output:
     - `Trace option error: Invalid trace selector: '*' (did you mean '.*'?)`
     - `Trace selector examples:`
   - Must not include:
     - `Trace logging options:`

Run only these tests:

```bash
ctest --test-dir demo/executable/build/latest --output-on-failure -R demo_cli_
```
