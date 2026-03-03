# Selector Enable/Disable Semantics Alignment

## Overview
- Current selector behavior does not match intended API semantics for `EnableChannels(...)` and `DisableChannels(...)`.
- Intention: list APIs should expand into concrete fully qualified channels and apply explicit per-channel enable/disable operations (equivalent to calling `EnableChannel(...)` / `DisableChannel(...)` one-by-one).
- Current implementation stores selector patterns directly and resolves conflicts by evaluation order, which can create precedence surprises.

## Current Behavior (As Implemented)
- `EnableChannels(...)` parses selector expressions and stores them as selector rules.
- `DisableChannels(...)` does the same for disabled selector rules.
- Runtime matching checks disabled selectors first, then enabled selectors.
- Enabling removes only exact-equal disabled selectors, not broader matching disabled patterns.

## Intended Behavior
- Channels are disabled by default.
- Developers explicitly turn channels on/off.
- `EnableChannels(...)` and `DisableChannels(...)` should behave like list conveniences over explicit per-channel calls, not long-lived overlapping pattern rules with precedence concerns.
- `EnableChannels(...)` should parse selector inputs containing `*`, brace groups (`{...}`), and comma-separated values, expand them to fully qualified channel names, and call `EnableChannel(...)` once per resolved channel.
- `DisableChannels(...)` should follow the same model and call `DisableChannel(...)` once per resolved channel.

## Why This Matters
- With the current rule-based approach, broad disable patterns can continue to block narrower enables unless they are exact selector matches.
- This creates behavior that feels implicit rather than explicit, and may conflict with developer expectations.

## Suggested Follow-up Work
- Decide whether list APIs must resolve selectors to concrete registered channels at call time.
- If yes, implement expansion + per-channel application semantics.
- Add tests covering:
  - default-disabled behavior
  - `EnableChannel`/`DisableChannel` exact behavior
  - `EnableChannels`/`DisableChannels` equivalence to repeated single-channel calls
  - wildcard/brace list inputs after expansion
- Keep this work separate from build-script stabilization work.
