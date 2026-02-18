# src/game/ui/config/AGENTS.md

Read `src/game/ui/AGENTS.md` first.
This directory contains **UI config accessors** and shared mapping utilities.

## Key files
- `ui_config.*` — typed wrapper around ConfigStore for UI settings.
- `render_settings.*` — brightness state handling.
- `hud_settings.*` — HUD visibility toggles.
- `render_scale.*` — UI render scale support.
- `input_mapping.*` — map window input to UI backends (`ui::input::mapping`).

## How it connects
Frontends and controllers use these helpers to avoid direct ConfigStore access.
