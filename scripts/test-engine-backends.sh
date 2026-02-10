#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

TEST_REGEX="physics_backend_parity_jolt|physics_backend_parity_physx|audio_backend_smoke_sdl3audio|audio_backend_smoke_miniaudio"

cd "${REPO_ROOT}"

cmake -S . -B build-dev

cmake --build build-dev --target \
  physics_backend_parity_test \
  audio_backend_smoke_test

REGISTERED_COUNT="$(
  ctest --test-dir build-dev -N -R "${TEST_REGEX}" \
    | awk '/Total Tests:/ {print $3}'
)"

if [[ -z "${REGISTERED_COUNT}" ]]; then
  echo "Unable to determine registered engine backend test count."
  exit 1
fi

if [[ "${REGISTERED_COUNT}" == "0" ]]; then
  echo "No engine backend tests are registered in this build profile; skipping CTest run."
  exit 0
fi

ctest --test-dir build-dev -R "${TEST_REGEX}" --output-on-failure
