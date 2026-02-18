# Rewrite Overseer Mode

This folder is for multi-repo integration workflow

Use this setup when you need integration-level oversight across all repos:

1. Create a workspace directory:
   - `mkdir -p ~/dev/bz3-rewrite`
   - `cd ~/dev/bz3-rewrite`
2. Clone `q-karma`:
   - `git clone --branch main https://github.com/QM-Code/karma.git q-karma`
3. Clone `m-dev`:
   - `git clone --branch m-dev https://github.com/QM-Code/bz3.git m-dev`
4. Clone `m-rewrite`:
   - `git clone --branch m-rewrite https://github.com/QM-Code/bz3.git m-rewrite`
5. Initialize workspace bootstrap:
   - `cp m-rewrite/docs/overseer/README.init README.md`
6. Start Codex from `~/dev/bz3-rewrite/` and follow that README.

