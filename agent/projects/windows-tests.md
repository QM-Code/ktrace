# Windows Test Portability

Status: deferred
Created: 2026-03-10

## Goal

Make the test suite runnable on native Windows without needing to rediscover the current blockers.

This should cover:

- root C++ tests under `cmake/tests/`
- demo CLI tests under `demo/exe/*/cmake/tests/`

Linux validation was already done during bootstrap on 2026-03-10:

- `./kbuild.py --help`
- `./kbuild.py --build-latest`
- `./build/latest/cmake/tests/ktrace_format_api_test`
- `./demo/exe/core/build/latest/test --trace '.*'`

Current Linux behavior is healthy. The work here is portability, not an existing Linux regression.

## Confirmed Windows blockers

### 1. Root C++ tests hard-fail on `_WIN32`

These two tests duplicate a POSIX-only `StdoutCapture` helper:

- `cmake/tests/ktrace_log_api_test.cpp`
- `cmake/tests/ktrace_format_api_test.cpp`

Relevant code:

- `cmake/tests/ktrace_log_api_test.cpp:35`
- `cmake/tests/ktrace_format_api_test.cpp:35`

Current behavior:

- POSIX path uses `std::tmpfile()`, `dup`, `dup2`, `close`, and `fileno`
- Windows path immediately calls `Fail("stdout capture is not implemented on Windows")`

These tests are still registered unconditionally in:

- `cmake/tests/CMakeLists.txt:33`
- `cmake/tests/CMakeLists.txt:49`

Why capture is needed:

- `ktrace` writes directly to `stdout` using `std::fwrite(...)` in `src/ktrace.cpp:36`
- this means normal `std::ostringstream` redirection is not enough

### 2. Demo CLI tests are bash scripts invoked directly by CTest

Current registration:

- `demo/exe/core/cmake/tests/CMakeLists.txt:1`
- `demo/exe/omega/cmake/tests/CMakeLists.txt:1`

Current scripts:

- `demo/exe/core/cmake/tests/trace_cli_cases.sh`
- `demo/exe/omega/cmake/tests/trace_cli_cases.sh`

Why this blocks native Windows:

- scripts require `bash`
- assertions use `grep`
- scripts use bash features such as arrays, here-strings, `[[ ... ]]`, and `set -euo pipefail`
- CTest is invoking the shell script directly rather than calling a cross-platform runner

## Recommended implementation path

### Phase 1: make the two root C++ tests portable

Preferred change:

- extract `StdoutCapture` into a shared helper used by both tests
- keep the current POSIX implementation
- add a Windows CRT implementation

Windows APIs to use:

- `_dup`
- `_dup2`
- `_close`
- `_fileno`
- `tmpfile_s` or `std::tmpfile()` if it behaves correctly in the target environment

Likely helper location:

- `cmake/tests/stdout_capture.hpp`
- or `cmake/tests/test_support/stdout_capture.hpp`

Expected behavior:

- redirect the real `stdout` file descriptor
- run the logger/test code
- restore `stdout`
- read the captured temp file back into a string

Useful product-code context:

- all logger output goes through `emitLine(...)` in `src/ktrace.cpp:36`
- writes are serialized by `LoggerData::output_mutex`

Good cleanup while doing this:

- remove the duplicated capture implementation from both tests
- normalize `\r\n` to `\n` in the helper or in test assertions so output matching stays stable across platforms

### Phase 2: replace bash demo tests with something CTest can run on Windows

Preferred approach:

- replace the `.sh` scripts with native CTest assertions
- invoke the demo binaries directly with `add_test(...)`
- use CTest properties such as:
  - `PASS_REGULAR_EXPRESSION`
  - `FAIL_REGULAR_EXPRESSION`
  - `WILL_FAIL`

Why this is preferred:

- no shell dependency
- no Git Bash assumption
- no `grep` dependency
- fewer moving parts than keeping script wrappers

Fallback approach if pure CTest becomes too awkward:

- write a tiny compiled C++ test runner that:
  - launches the demo binary
  - captures stdout/stderr
  - checks exit status
  - performs contains/regex assertions

Avoid if possible:

- maintaining parallel `.sh` and `.ps1` test scripts
- requiring Git Bash just for tests

## Existing demo cases to preserve

Core demo cases from `demo/exe/core/cmake/tests/trace_cli_cases.sh`:

- `unknown_option`
- `blank_trace`
- `timestamps_option`
- `imported_selector`

Omega demo cases from `demo/exe/omega/cmake/tests/trace_cli_cases.sh`:

- `unknown_option`
- `blank_trace`
- `bad_selector`
- `exact_selector_warning`
- `wildcard_selector_warning`
- `timestamps_option`
- `files_option`
- `functions_option`
- `functions_with_timestamps`
- `removed_lines_option`
- `wildcard_all_depth3`
- `brace_selector`

When rewriting, preserve the same semantic assertions:

- expected exit code
- expected text fragments
- expected regex matches
- explicit non-matches

## Important output details for Windows

### Newlines

Windows may produce `\r\n` in captured output. If the assertions keep using raw string matching, normalize line endings first.

### Colors

Current code disables ANSI color support on Windows:

- `src/ktrace/format.cpp:124`

That is helpful because the output checks do not need to strip ANSI escape sequences on Windows.

### Formatting expectations

Recent work removed the `spdlog/fmt` dependency and replaced it with an in-tree formatter:

- `include/ktrace.hpp:71`
- `src/ktrace.cpp:68`

The current formatter supports:

- sequential `{}`
- escaped braces `{{` and `}}`

It does not support fmt-style format specifiers such as `{:x}`. The new format test currently expects `{:x}` to throw:

- `cmake/tests/ktrace_format_api_test.cpp:129`

This is not a Windows-specific blocker, but it is part of the current test baseline and should not be changed accidentally while porting tests.

## Suggested work sequence

1. Extract and port `StdoutCapture` for the root C++ tests.
2. Run `./kbuild.py --build-latest` on Linux to confirm no regression.
3. Replace demo shell tests with native CTest-based tests.
4. Re-run the demo cases locally on Linux.
5. Run the full test flow on Windows.

## Suggested acceptance criteria

- `cmake/tests/ktrace_log_api_test.cpp` passes on native Windows
- `cmake/tests/ktrace_format_api_test.cpp` passes on native Windows
- demo CLI test coverage remains equivalent to current shell coverage
- no bash dependency is required for Windows test execution
- Linux behavior remains unchanged

## Likely files to touch

- `cmake/tests/CMakeLists.txt`
- `cmake/tests/ktrace_log_api_test.cpp`
- `cmake/tests/ktrace_format_api_test.cpp`
- `demo/exe/core/cmake/tests/CMakeLists.txt`
- `demo/exe/omega/cmake/tests/CMakeLists.txt`
- possibly new shared test helper files under `cmake/tests/`
- possibly removal of:
  - `demo/exe/core/cmake/tests/trace_cli_cases.sh`
  - `demo/exe/omega/cmake/tests/trace_cli_cases.sh`

## Notes for the next pass

- The repo had unrelated uncommitted changes during the bootstrap pass. Do not assume a clean worktree.
- No code changes were made for this note.
- Start by implementing the shared stdout capture helper first; that is the smallest contained piece.
