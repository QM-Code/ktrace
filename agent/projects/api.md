# Public API reduction

## Overview

Please look through the files in m-karma/include/. That is supposed to be the KarmaSDK public API, but there is a lot of stuff in there that is not being used by anything public. We need to go through it in depth and move everything that is not being used either either (1) m-bz3/src/ or (2) m-karma/demo/**/src/ to appropriate places in m-karma/src/

## Phases

### Phase 1

Look through m-karma/demo/**/src/ and m-bz3/src/ and create a list of all the KarmaSDK headers/functions being used. Add that list to project file api/findings.txt

### Phase 2

Go through the m-karma/include/ directory and find every header/function that is not being used by m-karma/demo/**/src/ or m-bz3/src/. Add that list to project file api/findings.txt

### Phase 3

Move all unused headers/functions from m-karma/include/ to an appropriate place in m-karma/src/. As items are moved, put [DONE] next to relevant entries in api/findings.txt

## Progress

### Phase 1

Completed (analysis only; no code changes).

See `m-overseer/agent/projects/api/findings.txt`:
- `Phase 1 - KarmaSDK headers used (exact include sites)`
- `Phase 1 - KarmaSDK functions used (heuristic)`
- `Phase 1 - Includes seen in scope but not from m-karma/include`

Summary from findings:
- Public headers in `m-karma/include`: 95
- Public headers directly used in scope: 57
- Include sites referencing public headers: 236
- Functions detected as used (heuristic): 176

### Phase 2

Completed (analysis only; no code changes).

See `m-overseer/agent/projects/api/findings.txt`:
- `Phase 2 - KarmaSDK headers not used (exact include-based)`
- `Phase 2 - KarmaSDK functions not used in otherwise-used headers (heuristic)`

Summary from findings:
- Public headers not used in scope: 38
- Functions detected as unused within otherwise-used headers (heuristic): 205

### Phase 3

Completed for the header-level migration set (no compatibility shims added).

Result:
- All headers listed under `Phase 2 - KarmaSDK headers not used (exact include-based)` in `api/findings.txt` are now marked `[DONE]`.
- The previously coupled set was collapsed into still-used public headers where needed, then moved to `m-karma/src/karma/...`.
- `m-karma/include/karma` now contains 56 public headers, and all 56 are directly used by `m-bz3/src` or `m-karma/demo/**/src`.
