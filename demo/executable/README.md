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

- `cmake/tests/trace_cli_cases.sh`

When `BUILD_TESTING=ON`, `CMakeLists.txt` registers these CTest tests:

1. `demo_cli_unknown_trace_option_shows_help`
   - Purpose: ensure unknown `--trace-*` options show usage hint output.
   - Command under test: `./test --trace-f`
   - Required output:
     - `unknown option --trace-f (use --trace to list options)`
   - Must not include:
     - `Trace logging options:`
     - `Trace selector examples:`

2. `demo_cli_blank_trace_shows_help`
   - Purpose: ensure blank `--trace` prints help.
   - Command under test: `./test --trace`
   - Required output:
     - `Available --trace-* options:`
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

4. `demo_cli_wildcard_all_includes_depth3`
   - Purpose: ensure wildcard-all selector includes nested depth-3 channels.
   - Command under test: `./test --trace '*.*.*.*'`
   - Required output:
     - `executable trace test on channel 'deep.branch.leaf'`
     - `alpha trace test on channel 'net'`
     - `beta trace test on channel 'io'`
     - `gamma trace test on channel 'physics'`
   - Must not include:
     - `Trace option error:`

5. `demo_cli_brace_selector_enables_expected_channels`
   - Purpose: ensure brace selectors enable only expected channels.
   - Command under test: `./test --trace '*.{net,io}'`
   - Required output:
     - `alpha trace test on channel 'net'`
     - `beta trace test on channel 'io'`
   - Must not include:
     - `alpha trace test on channel 'cache'`
     - `beta trace test on channel 'scheduler'`
     - `gamma trace test on channel 'physics'`
     - `gamma trace test on channel 'metrics'`

Run only these tests:

```bash
ctest --test-dir demo/executable/build/latest --output-on-failure -R demo_cli_
```
