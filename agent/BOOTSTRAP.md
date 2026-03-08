# Coding Agent Bootstrap

## Overview

Familiarize yourself with this project by reading:

- README.md
- CMakeLists.txt
- src/*               Source tree
- include/*           Public API (if present)

## Projects

Ongoing projects can be found in `agent/projects/*.md`.

If any projects are found, present them to the operator after bootstrap is complete.

## Issues / Recommendations

If you notice issues or have recommendations about the codebase, bring them to the operator.

## Building with kbuild.py

- Always use `kbuild.py` for builds. Do not use raw `cmake` commands for normal build flows.
- Always run from the repo root, invoking `./kbuild.py` from that directory.
- Make a test run of `./kbuild.py --help`.
- `./kbuild.py` with no arguments also prints usage; it does not build.
- `./kbuild.py --build-latest` builds the core SDK/app into `build/latest/` and then builds demos listed in `kbuild.json -> build.defaults.demos` (if defined).
- Use `./kbuild.py --build-demos [demo ...]` for explicit demo builds.
- With no demo names, `--build-demos` uses `kbuild.json -> build.demos`.
- Use `./kbuild.py --clean <version>` or `./kbuild.py --clean-latest` when you need a fresh build tree.

## Testing

- Prefer end-to-end checks using demo binaries under `demo/*/build/<version>/`.
- Add scripted test cases for demo usage under `tests/` or `cmake/tests/` as appropriate.
- Keep unit-style tests focused and explicit.

## Rules and Regulations

- **Always plan first**
- **Discuss, then code**
- Do not jump straight to coding when given a prompt. Consult with the operator and propose a structured plan before making code changes.
- If you have not been given an explicit instruction to begin coding, do not start coding.
- Do not interpret questions starting with "Can I/we/you...?" or "Is it possible to...?" as instructions to begin coding. Answer first, then ask whether to proceed.
- Always provide a summary of changes when you are finished.
