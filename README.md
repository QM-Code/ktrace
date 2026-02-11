# m-rewrite (Tracked Control Plane + Production Rewrite)

If you are reading this file directly, your workspace is probably incomplete.

Use this setup:

1. Create an empty workspace directory:
   - `mkdir -p ~/dev/bz3-rewrite`
   - `cd ~/dev/bz3-rewrite`
2. Clone `KARMA-REPO` (main):
   - `git clone --branch main https://github.com/QM-Code/karma.git KARMA-REPO`
3. Clone `m-dev` (source-of-truth behavior baseline):
   - `git clone --branch m-dev https://github.com/QM-Code/bz3.git m-dev`
4. Clone `m-rewrite` (active implementation branch):
   - `git clone --branch m-rewrite https://github.com/QM-Code/bz3.git m-rewrite`
5. Initialize the root bootstrap README:
   - `cp m-rewrite/README.init README.md`
6. Start Codex from `~/dev/bz3-rewrite/` and run the instructions in `README.md`.

Notes:
- All canonical overseer/specialist docs are tracked under `m-rewrite/AGENTS.md` and `m-rewrite/docs/`.
- `m-dev` is behavior parity reference; `KARMA-REPO` is capability-intake reference.
