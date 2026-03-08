#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <demo-binary> <case>"
    echo "Cases:"
    echo "  unknown_option"
    echo "  blank_trace"
    echo "  timestamps_option"
    echo "  imported_selector"
}

if [[ $# -ne 2 ]]; then
    usage
    exit 2
fi

binary="$1"
test_case="$2"

if [[ ! -x "$binary" ]]; then
    echo "Error: demo binary is not executable: $binary" >&2
    exit 2
fi

require_contains() {
    local output="$1"
    local needle="$2"
    if ! grep -Fq "$needle" <<<"$output"; then
        echo "Expected output to contain: $needle" >&2
        echo "--- output begin ---" >&2
        echo "$output" >&2
        echo "--- output end ---" >&2
        exit 1
    fi
}

require_not_contains() {
    local output="$1"
    local needle="$2"
    if grep -Fq "$needle" <<<"$output"; then
        echo "Expected output to not contain: $needle" >&2
        echo "--- output begin ---" >&2
        echo "$output" >&2
        echo "--- output end ---" >&2
        exit 1
    fi
}

require_regex() {
    local output="$1"
    local pattern="$2"
    if ! grep -Eq "$pattern" <<<"$output"; then
        echo "Expected output to match regex: $pattern" >&2
        echo "--- output begin ---" >&2
        echo "$output" >&2
        echo "--- output end ---" >&2
        exit 1
    fi
}

run_case() {
    local -a args=("$@")
    "$binary" "${args[@]}" 2>&1 || true
}

case "$test_case" in
    unknown_option)
        output="$(run_case --trace-f)"
        require_contains "$output" "unknown option --trace-f (use --trace to list options)"
        require_not_contains "$output" "--trace-help"
        require_not_contains "$output" "Trace logging options:"
        require_not_contains "$output" "Trace selector examples:"
        ;;
    blank_trace)
        output="$(run_case --trace)"
        require_contains "$output" "Available --trace-* options:"
        require_not_contains "$output" "--trace-help"
        require_not_contains "$output" "Trace option error:"
        require_not_contains "$output" "Trace selector examples:"
        ;;
    timestamps_option)
        output="$(run_case --trace ".app" --trace-timestamps)"
        require_regex "$output" "\\[core\\] \\[[0-9]+\\.[0-9]{6}\\] \\[app\\] cli processing enabled, use --trace for options"
        require_not_contains "$output" "Trace option error:"
        ;;
    imported_selector)
        output="$(run_case --trace "*.*")"
        require_not_contains "$output" "Trace option error:"
        require_contains "$output" "testing imported tracing, use --trace '*.*' to view imported channels"
        require_contains "$output" "alpha trace test on channel 'net'"
        ;;
    *)
        echo "Error: unknown case '$test_case'" >&2
        usage
        exit 2
        ;;
esac

echo "PASS: $test_case"
