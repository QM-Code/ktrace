## Repo/Branch Layout

The multi-repo project root is located at <root>.

Only the following files and/or subdirectories should exist:

- README.md
  - Instructions to human operators.

- m-overseer/
  - The overseer's directory (i.e. your directory), where you manage project files and get your bootstrap information.
  - This is tracked and periodically pushed to ensure copies of bootstrap files and project docs exist offsite.
  - If missing (should never happen), git clone --branch m-overseer https://github.com/QM-Code/bz3.git m-overseer
    - This really should never happen, because how else are you reading this BOOTSTRAP.md file???
	- If somehow you have read this file but this directory is missing, please stop everything you are doing and notify the human operator immediately.

- m-karma/
  - This is the game development kit, often referred to as the "engine" or "sdk".
  - It contains as much game-agnostic code as possible to make game development (e.g. m-bz3) simple.
  - It exports an SDK library and headers to be used by game developers (e.g. m-bz3)
  - If missing, git clone --branch m-karma https://github.com/QM-Code/bz3.git m-karma
    - Ask user first before cloning

- m-bz3/
  - This is the "BZ3 - BZflag Revisited" game.
  - It builds on the m-karma SDK libraries and headers.
  - If missing, git clone --branch m-bz3 https://github.com/QM-Code/bz3.git m-bz3
    - Ask user first before cloning

- q-karma/
  - Experimental code primarily concerned with rendering and physics backends
  - We use this as a source of new features and architectural guidance to integrate into m-karma
  - We will continue to monitor for changes periodically and integrate useful code into m-karma
  - If missing, git clone --branch main https://github.com/QM-Code/karma.git q-karma
    - Ask user first before cloning

 
- m-dev/
  - An old development branch that managed to get a lot of functional gameplay.
  - Was abandoned because the code was totally chaotic.
  - q-karma developed the "right" way to build and engine/game infrastructure (Unity/Unreal/Godot model)
  - We have extracted most gameplay and UI features from m-dev and will stop following it once parity is reached.
  - If missing, git clone --branch m-dev https://github.com/QM-Code/bz3.git m-dev
    - Ask user first before cloning

- If any other files or directories exist, notify the human operator immediately.
- If any of these directories do not exist, notify the human operator immediately.
