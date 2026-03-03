# Dead Code Review Notes

## Scope
- Review `src/` for dead code.
- Treat code in `src/` whose only purpose is to support tests as dead code.

## Current Result
- No true dead code found in `src/`.
- No `src/` symbols found that are only used by tests.
- Compiler check with unused warnings did not report unused `src/` functions/variables.

## Cleanup Candidates (Not Dead, but worth revisiting)
- `src/ktrace.hpp`: `parseSelectorChannelPattern`, `parseSelectorExpression`
  - Used only by `src/ktrace/selectors.cpp`.
  - Candidate to make file-local in `selectors.cpp`.
- `src/ktrace.hpp`: `appendCompactTimestamp`, `formatSourceLabel`
  - Used only by `src/ktrace/format.cpp`.
  - Candidate to make file-local in `format.cpp`.
- `src/ktrace.cpp`: `EnableInternalTrace()`
  - Often redundant in current flows, but part of the public API contract.
  - Keep as API surface; not dead.

## Follow-up
- Revisit after broader API/build refactors.
