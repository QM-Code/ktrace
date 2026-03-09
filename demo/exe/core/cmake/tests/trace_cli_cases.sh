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
    if ! grep -Fq -- "$needle" <<<"$output"; then
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
    if grep -Fq -- "$needle" <<<"$output"; then
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
    if ! grep -Eq -- "$pattern" <<<"$output"; then
        echo "Expected output to match regex: $pattern" >&2
        echo "--- output begin ---" >&2
        echo "$output" >&2
        echo "--- output end ---" >&2
        exit 1
    fi
}

run_case() {
    local -a args=("$@")
    local output
    set +e
    output="$("$binary" "${args[@]}" 2>&1)"
    local status=$?
    set -e
    echo "$status"$'\n'"$output"
}

run_and_split() {
    local payload
    payload="$(run_case "$@")"
    status="$(head -n1 <<<"$payload")"
    output="$(tail -n +2 <<<"$payload")"
}

status=0
output=""

case "$test_case" in
    unknown_option)
        run_and_split --trace-f
        if [[ "$status" -eq 0 ]]; then
            echo "Expected non-zero exit status for unknown trace option" >&2
            exit 1
        fi
        require_contains "$output" "CLI error: unknown option --trace-f"
        require_not_contains "$output" "Available --trace-* options:"
        ;;
    blank_trace)
        run_and_split --trace
        if [[ "$status" -ne 0 ]]; then
            echo "Expected zero exit status for bare trace root" >&2
            echo "--- output begin ---" >&2
            echo "$output" >&2
            echo "--- output end ---" >&2
            exit 1
        fi
        require_contains "$output" "Available --trace-* options:"
        require_contains "$output" "--trace <channels>"
        require_not_contains "$output" "CLI error:"
        require_not_contains "$output" "Trace selector examples:"
        ;;
    timestamps_option)
        run_and_split --trace ".app" --trace-timestamps
        if [[ "$status" -ne 0 ]]; then
            echo "Expected zero exit status for timestamps option" >&2
            echo "--- output begin ---" >&2
            echo "$output" >&2
            echo "--- output end ---" >&2
            exit 1
        fi
        require_regex "$output" "\\[core\\] \\[[0-9]+\\.[0-9]{6}\\] \\[app\\] cli processing enabled, use --trace for options"
        require_not_contains "$output" "CLI error:"
        ;;
    imported_selector)
        run_and_split --trace "*.*"
        if [[ "$status" -ne 0 ]]; then
            echo "Expected zero exit status for imported selector" >&2
            echo "--- output begin ---" >&2
            echo "$output" >&2
            echo "--- output end ---" >&2
            exit 1
        fi
        require_not_contains "$output" "CLI error:"
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
