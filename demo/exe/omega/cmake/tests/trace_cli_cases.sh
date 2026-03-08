#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <demo-binary> <case>"
    echo "Cases:"
    echo "  unknown_option"
    echo "  blank_trace"
    echo "  bad_selector"
    echo "  exact_selector_warning"
    echo "  wildcard_selector_warning"
    echo "  timestamps_option"
    echo "  files_option"
    echo "  functions_option"
    echo "  functions_requires_files_warning"
    echo "  removed_lines_option"
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
    bad_selector)
        output="$(run_case --trace "*")"
        require_contains "$output" "[omega] [error] Trace option error: Invalid trace selector: '*' (did you mean '.*'?)"
        require_contains "$output" "Trace selector examples:"
        require_not_contains "$output" "Trace logging options:"
        ;;
    exact_selector_warning)
        output="$(run_case --trace ".missing")"
        require_contains "$output" "[omega] [warning] enable ignored channel selector 'omega.missing' because it matched no registered channels"
        require_not_contains "$output" "Trace option error:"
        ;;
    wildcard_selector_warning)
        output="$(run_case --trace "missing.*")"
        require_contains "$output" "[omega] [warning] enable ignored channel selector 'missing.*' because it matched no registered channels"
        require_not_contains "$output" "Trace option error:"
        ;;
    timestamps_option)
        output="$(run_case --trace ".app" --trace-timestamps)"
        require_regex "$output" "\\[omega\\] \\[[0-9]+\\.[0-9]{6}\\] \\[app\\] cli processing enabled, use --trace for options"
        require_regex "$output" "\\[omega\\] \\[[0-9]+\\.[0-9]{6}\\] \\[app\\] testing external tracing, use --trace '\\*\\.\\*' to view top-level channels"
        ;;
    files_option)
        output="$(run_case --trace ".app" --trace-files)"
        require_regex "$output" "\\[omega\\] \\[app\\] \\[main:[0-9]+\\] cli processing enabled, use --trace for options"
        require_regex "$output" "\\[omega\\] \\[app\\] \\[main:[0-9]+\\] testing external tracing, use --trace '\\*\\.\\*' to view top-level channels"
        ;;
    functions_option)
        output="$(run_case --trace ".app" --trace-files --trace-functions)"
        require_regex "$output" "\\[omega\\] \\[app\\] \\[main:[0-9]+:main\\] cli processing enabled, use --trace for options"
        require_regex "$output" "\\[omega\\] \\[app\\] \\[main:[0-9]+:main\\] testing external tracing, use --trace '\\*\\.\\*' to view top-level channels"
        require_not_contains "$output" "requires --trace-files to be operational"
        ;;
    functions_requires_files_warning)
        output="$(run_case --trace ".app" --trace-timestamps --trace-functions)"
        require_regex "$output" "\\[omega\\] \\[[0-9]+\\.[0-9]{6}\\] \\[warning\\] --trace-functions requires --trace-files to be operational"
        require_regex "$output" "\\[omega\\] \\[[0-9]+\\.[0-9]{6}\\] \\[app\\] cli processing enabled, use --trace for options"
        require_not_contains "$output" "[main:"
        ;;
    removed_lines_option)
        output="$(run_case --trace ".app" --trace-lines)"
        require_contains "$output" "unknown option --trace-lines (use --trace to list options)"
        require_not_contains "$output" "Trace option error:"
        ;;
    wildcard_all_depth3)
        output="$(run_case --trace "*.*.*.*")"
        require_not_contains "$output" "Trace option error:"
        require_contains "$output" "omega trace test on channel 'deep.branch.leaf'"
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
