#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

TEST_REGEX="server_net_contract_test|server_runtime_event_rules_test|enet_environment_probe_test|enet_loopback_integration_test|enet_multiclient_loopback_test|enet_disconnect_lifecycle_integration_test|client_world_package_safety_integration_test|client_transport_contract_test|server_transport_contract_test"
BUILD_DIR="build-dev"

usage() {
  cat <<EOF
Usage: $(basename "$0") [build-dir]

Runs server/network validation tests in the provided build directory.
Default build directory: build-dev
EOF
}

if [[ $# -gt 1 ]]; then
  usage >&2
  exit 1
fi

if [[ $# -eq 1 ]]; then
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    -*)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
    *)
      BUILD_DIR="$1"
      ;;
  esac
fi

cd "${REPO_ROOT}"

"${REPO_ROOT}/scripts/check-network-backend-encapsulation.sh"

cmake -S . -B "${BUILD_DIR}"

cmake --build "${BUILD_DIR}" --target \
  server_net_contract_test \
  server_runtime_event_rules_test \
  enet_environment_probe_test \
  enet_loopback_integration_test \
  enet_multiclient_loopback_test \
  enet_disconnect_lifecycle_integration_test \
  client_world_package_safety_integration_test \
  client_transport_contract_test \
  server_transport_contract_test

ctest --test-dir "${BUILD_DIR}" -R "${TEST_REGEX}" --output-on-failure
