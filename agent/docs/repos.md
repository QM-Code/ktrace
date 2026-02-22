## Repo/Branch Layout

The multi-repo project root is located at <root>.

Only the following files and/or subdirectories should exist:

- README.md
  - Instructions to human operators.

- m-overseer/
  - The overseer's directory (i.e. your directory), where you manage project files and get your bootstrap information.
  - This is tracked and periodically pushed to ensure copies of bootstrap files and project docs exist offsite.
  - If missing (this should never happen), git clone --branch m-overseer https://github.com/QM-Code/bz3.git m-overseer
    - This reall should never happen, because how else are you reading this BOOTSTRAP.md file???

- m-karma/
  - This is the game devleopment kit, often referred to as the "engine".
  - It contains as much game-agnostic code as possible to make game development (e.g. m-bz3) simple.
  - It exports a SDK library and headers to be used by game developers (e.g. m-bz3)
  - If missing, git clone --branch m-karma https://github.com/QM-Code/bz3.git m-karma
    - Ask user first

- m-bz3/
  - This is the "BZ3 - BZflag Revistited" game.
  - It builds on the m-karma SDK libraries and headers.
  - git clone --branch m-bz3 https://github.com/QM-Code/bz3.git m-bz3
    - Ask user first

- q-karma/
  - Experimental code primarily concerned with rendering and physics backends
  - We use this as a source of new features and architectural guidance to integrate into m-karma
  - We will continue to monitor for changes periodically and integrate useful code into m-karma
  - If missing, git clone --branch main https://github.com/QM-Code/karma.git q-karma
    - Ask user first

 
- m-dev/
  - An old development branch that managed to get a lot of functional gameplay.
  - Was abandoned because the code was totally chaotic.
  - q-karma developed the "right" way to build and engine/game infrastructure (Unity/Unreal/Godot model)
  - We have extracted most gameplay and UI features from m-dev and will stop following it once parity is reached.
  - If missing,  git clone --branch m-dev https://github.com/QM-Code/bz3.git m-dev
    - Ask user first

- If any other files or directories exist, notify the human operator immediately.
- If any of these directories do not exist, notify the human operator immediatley.

