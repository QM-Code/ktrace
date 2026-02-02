# src/game/client/architecture.md

Client flow:
1) `main.cpp` builds the engine and starts client loop.
2) `Game` creates world session and player (unless roaming).
3) `Player` updates camera position/rotation each frame.
4) `RoamingCameraController` applies a free camera when roaming.
5) UI is driven by engine UI system; game handles chat input.

Input gating:
- Gameplay input (movement/fire/spawn/jump) is suppressed when UI input is active
  (console visible or chat focused).
- Global actions (chat/escape/quick quit/fullscreen) remain active.
EngineApp now pumps window events and drives UI updates; client code consumes
the resulting input state.
