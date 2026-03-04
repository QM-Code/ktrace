#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <demo-binary> <case>"
    echo "Cases:"
    echo "  unknown_option"
    echo "  blank_trace"
    echo "  bad_selector"
    echo "  wildcard_all_depth3"
    echo "  brace_selector"
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

run_case() {
    local -a args=("$@")
    "$binary" "${args[@]}" 2>&1 || true
}

case "$test_case" in
    unknown_option)
        output="$(run_case --trace-f)"
        require_contains "$output" "Trace option error: unknown trace option '--trace-f'"
        require_contains "$output" "Trace logging options:"
        require_not_contains "$output" "Trace selector examples:"
        ;;
    blank_trace)
        output="$(run_case --trace)"
        require_contains "$output" "Trace logging options:"
        require_not_contains "$output" "Trace option error:"
        require_not_contains "$output" "Trace selector examples:"
        ;;
    bad_selector)
        output="$(run_case --trace "*")"
        require_contains "$output" "Trace option error: Invalid trace selector: '*' (did you mean '.*'?)"
        require_contains "$output" "Trace selector examples:"
        require_not_contains "$output" "Trace logging options:"
        ;;
    wildcard_all_depth3)
        output="$(run_case --trace "*.*.*.*")"
        require_not_contains "$output" "Trace option error:"
        require_contains "$output" "executable trace test on channel 'deep.branch.leaf'"
        require_contains "$output" "alpha trace test on channel 'net'"
        require_contains "$output" "beta trace test on channel 'io'"
        require_contains "$output" "gamma trace test on channel 'physics'"
        ;;
    brace_selector)
        output="$(run_case --trace "*.{net,io}")"
        require_not_contains "$output" "Trace option error:"
        require_contains "$output" "alpha trace test on channel 'net'"
        require_contains "$output" "beta trace test on channel 'io'"
        require_not_contains "$output" "alpha trace test on channel 'cache'"
        require_not_contains "$output" "beta trace test on channel 'scheduler'"
        require_not_contains "$output" "gamma trace test on channel 'physics'"
        require_not_contains "$output" "gamma trace test on channel 'metrics'"
        ;;
    *)
        echo "Error: unknown case '$test_case'" >&2
        usage
        exit 2
        ;;
esac

echo "PASS: $test_case"
