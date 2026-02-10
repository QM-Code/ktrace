#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

TEST_REGEX="server_net_contract_test|server_runtime_event_rules_test|enet_environment_probe_test|enet_loopback_integration_test|enet_multiclient_loopback_test|enet_disconnect_lifecycle_integration_test|client_world_package_safety_integration_test"

cd "${REPO_ROOT}"

cmake -S . -B build-dev

cmake --build build-dev --target \
  server_net_contract_test \
  server_runtime_event_rules_test \
  enet_environment_probe_test \
  enet_loopback_integration_test \
  enet_multiclient_loopback_test \
  enet_disconnect_lifecycle_integration_test \
  client_world_package_safety_integration_test

ctest --test-dir build-dev -R "${TEST_REGEX}" --output-on-failure
