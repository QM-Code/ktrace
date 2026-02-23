# Build Policy

## Per-branch/repo building

- m-karma and m-bz3 both provide the abuild.py script for building
- m-overseer does not build anything
- m-dev and q-karma provide bzbuild.py, but generally we will never be building there
- Always use abuild.py for building.
- Always build from the branch/repo root.

## Toolchain Policy
- m-karma and m-bz3 each must have their own branch-level `./vcpkg`
- A missing/unbootstrapped local `./vcpkg` in either m-karma or m-bz3 is a blocker
- Do not improvise or attempt workarounds for vcpkg errors.
- On bootstrap, if a repo exists but vcpkg has not been set up:
  - Inform the human operator that builds will not work until vcpkg is set up.
  - Offer to set up vcpkg.

## Restrictions

- IMPORTANT: Never use raw `cmake -S/-B` except for very specific tests. In general, always use abuild.py.
- Build directories must always start with 'build-'. NO EXCEPTIONS.


## Basics of abuild.py

- abuild.py assigns agents their own build directories and handles all build configuration
- abuild.py ensures ownership by having agents claim and lock directories before usage
- Basic usage:
  - Claiming and locking a build directory:
    - ./abuild.py --agent <agent-name> -d <build-dir> --claim-lock
	- Note: If this fails, it means the build directory is already claimed
  - Building: 
    - ./abuild.py --agent <agent-name> -d <build-dir> --configure
	- Note: You may omit `-c/--configure` when intentionally reusing a configured build dir.
  - Releasing a build directory:
    - ./abuild.py --agent <agent-name> -d <build-dir> --release-lock


## Agent naming

- Specialists should receive their names from the project manager/overseer.


## SDK builds

- m-karma builds must specify an SDK output directory:
  - ./abuild.py -c -d <build-dir> --install-sdk <sdk-output-dir>
- m-bz3 builds must specify an SDK intake directory:
  - ./abuild.py -c -d <build-dir> --karma-sdk <karma-sdk-dir>
- These must align in practice: <sdk-output-dir> = <karma-sdk-dir>
- Real-world example:
  - m-karma:
    - `./abuild.py -a <agent> -c -d <build-dir> --install-sdk out/karma-sdk`
  - m-bz3:
    - `./abuild.py -a <agent> -c -d <build-dir> --karma-sdk ../m-karma/out/karma-sdk`



### Advanced SDK builds

- static SDK contract: `./abuild.py -c -d <build-dir> --sdk-linkage static --install-sdk <prefix>`
- mobile shared override (explicit-only): `./abuild.py -c -d <build-dir> --sdk-linkage shared --mobile-allow-shared`

## Backends

- m-karma (engine/sdk) has a branched backend structure
- Different subsystems can choose which infrastructure they build around
  - physics: jolt/physx
  - audio: sdl3audio/miniaudio
  - renderer: bgfx/diligent
  - ui: imgui/rmlui
- abuild.py allows you to select which backends to build around.
- You can also include multiple backends in a single build, allowing for runtime backend selection.
- The default backend selection can be seen by running `./abuild.py --defaults`

### Enabling alternate/multiple backends

- Use `--backends` for building non-default backend options
- IMPORTANT: Most agents should not use `--backends`, and just use the default build configuration.
- Only use `--backends` when making changes that affect multiple backends
- Example usage:
  - Build using the diligent backend instead of the bgfx backend:
    - ./abuild.py -c -d <build-dir> -b diligent
  - Build allowing bgfx/diligent to be selected at runtime
    - ./abuild.py -c -d <build-dir> -b bgfx,diligent
  - Build overriding ui backend to use imgui and allowing bgfx/diligent to be selected at runtime
    - ./abuild.py -c -d <build-dir> -b imgui,bgfx,diligent

### Backend bugs

- Combined renderer mode (`-b bgfx,diligent`) is Linux shared-mode only.
- Non-Linux targets and static SDK linkage must select one renderer backend.

