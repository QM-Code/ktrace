#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"

fail=0

check_no_matches() {
  local description="$1"
  local cmd="$2"
  echo "[guardrail] ${description}"
  if eval "${cmd}"; then
    echo "[guardrail] FAIL: ${description}" >&2
    fail=1
  fi
}

check_no_matches \
  "no ENet header includes under src/game/*" \
  "rg -n --color=never '#include\\s*<enet\\.h>|#include\\s*<enet/enet\\.h>' src/game"

check_no_matches \
  "no direct ENet C API calls under src/game/*" \
  "rg -n --color=never '\\benet_[A-Za-z0-9_]+\\s*\\(' src/game"

check_no_matches \
  "no ENet C API types under src/game/*" \
  "rg -n --color=never '\\bENet(Host|Peer|Event|Address|Packet)\\b' src/game"

check_no_matches \
  "no ENet macro tokens under src/game/*" \
  "rg -n --color=never '\\bENET_[A-Za-z0-9_]+\\b' src/game"

check_no_matches \
  "no direct enet/enet_static target_link_libraries in src/game/CMakeLists.txt" \
  "rg -n --color=never --pcre2 --multiline '(?s)target_link_libraries\\([^\\)]*\\b(enet_static|enet)\\b' src/game/CMakeLists.txt"

check_no_matches \
  "no file path containing enet under src/game/*" \
  "find src/game -type f -iname '*enet*' -print | grep -q ."

check_no_matches \
  "no backend-name token enet under src/game/*" \
  "rg -n --color=never -i 'enet' src/game"

if [[ ${fail} -ne 0 ]]; then
  exit 1
fi

echo "[guardrail] OK: network backend encapsulation checks passed."
