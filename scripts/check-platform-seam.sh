#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

ALLOWLIST=(
    "src/engine/platform/backends/window_sdl3.cpp"
    "src/engine/audio/backends/backend_sdl3audio_stub.cpp"
)

for file in "${ALLOWLIST[@]}"; do
    if [[ ! -f "$file" ]]; then
        echo "[seam] Missing allowlist file: $file"
        exit 2
    fi
done

is_allowed_file() {
    local file="$1"
    local allowed
    for allowed in "${ALLOWLIST[@]}"; do
        if [[ "$file" == "$allowed" ]]; then
            return 0
        fi
    done
    return 1
}

collect_matches() {
    local pattern="$1"
    rg -n --no-heading --color=never \
        --glob '*.[ch]' \
        --glob '*.cc' \
        --glob '*.cpp' \
        --glob '*.cxx' \
        --glob '*.hpp' \
        -e "$pattern" include src || true
}

filter_disallowed() {
    local line
    local file
    while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        file="${line%%:*}"
        if ! is_allowed_file "$file"; then
            echo "$line"
        fi
    done
}

run_check() {
    local label="$1"
    local pattern="$2"
    local -a violations=()

    mapfile -t violations < <(collect_matches "$pattern" | filter_disallowed | sort -u)
    if (( ${#violations[@]} == 0 )); then
        echo "[seam] $label: OK"
        return 0
    fi

    echo "[seam] $label: FAIL"
    printf '%s\n' "${violations[@]}"
    return 1
}

echo "[seam] Checking SDL platform seam boundaries"
printf '[seam] Allowlist:\n'
printf '  - %s\n' "${ALLOWLIST[@]}"

failed=0

if ! run_check "SDL header includes outside allowlist" '^[[:space:]]*#include[[:space:]]*[<"]SDL'; then
    failed=1
fi

if ! run_check "SDL symbol usage outside allowlist" '\bSDL_[A-Za-z0-9_]+\b'; then
    failed=1
fi

if (( failed != 0 )); then
    exit 1
fi

echo "[seam] OK: SDL headers/types are confined to allowlisted backend implementation files."
