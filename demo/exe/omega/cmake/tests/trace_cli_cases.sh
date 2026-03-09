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
    echo "  functions_with_timestamps"
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
        require_contains "$output" "[error] [cli] unknown option --trace-f"
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
    bad_selector)
        run_and_split --trace "*"
        if [[ "$status" -eq 0 ]]; then
            echo "Expected non-zero exit status for invalid selector" >&2
            exit 1
        fi
        require_contains "$output" "[error] [cli] option '--trace': Invalid trace selector: '*' (did you mean '.*'?)"
        require_not_contains "$output" "Trace selector examples:"
        ;;
    exact_selector_warning)
        run_and_split --trace ".missing"
        if [[ "$status" -ne 0 ]]; then
            echo "Expected zero exit status for unmatched exact selector" >&2
            echo "--- output begin ---" >&2
            echo "$output" >&2
            echo "--- output end ---" >&2
            exit 1
        fi
        require_contains "$output" "[omega] [warning] enable ignored channel selector 'omega.missing' because it matched no registered channels"
        require_not_contains "$output" "CLI error:"
        ;;
    wildcard_selector_warning)
        run_and_split --trace "missing.*"
        if [[ "$status" -ne 0 ]]; then
            echo "Expected zero exit status for unmatched wildcard selector" >&2
            echo "--- output begin ---" >&2
            echo "$output" >&2
            echo "--- output end ---" >&2
            exit 1
        fi
        require_contains "$output" "[omega] [warning] enable ignored channel selector 'missing.*' because it matched no registered channels"
        require_not_contains "$output" "CLI error:"
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
        require_regex "$output" "\\[omega\\] \\[[0-9]+\\.[0-9]{6}\\] \\[app\\] cli processing enabled, use --trace for options"
        require_regex "$output" "\\[omega\\] \\[[0-9]+\\.[0-9]{6}\\] \\[app\\] testing external tracing, use --trace '\\*\\.\\*' to view top-level channels"
        ;;
    files_option)
        run_and_split --trace ".app" --trace-files
        if [[ "$status" -ne 0 ]]; then
            echo "Expected zero exit status for files option" >&2
            echo "--- output begin ---" >&2
            echo "$output" >&2
            echo "--- output end ---" >&2
            exit 1
        fi
        require_regex "$output" "\\[omega\\] \\[app\\] \\[main:[0-9]+\\] cli processing enabled, use --trace for options"
        require_regex "$output" "\\[omega\\] \\[app\\] \\[main:[0-9]+\\] testing external tracing, use --trace '\\*\\.\\*' to view top-level channels"
        ;;
    functions_option)
        run_and_split --trace ".app" --trace-functions
        if [[ "$status" -ne 0 ]]; then
            echo "Expected zero exit status for functions option" >&2
            echo "--- output begin ---" >&2
            echo "$output" >&2
            echo "--- output end ---" >&2
            exit 1
        fi
        require_regex "$output" "\\[omega\\] \\[app\\] \\[main:[0-9]+:main\\] cli processing enabled, use --trace for options"
        require_regex "$output" "\\[omega\\] \\[app\\] \\[main:[0-9]+:main\\] testing external tracing, use --trace '\\*\\.\\*' to view top-level channels"
        ;;
    functions_with_timestamps)
        run_and_split --trace ".app" --trace-timestamps --trace-functions
        if [[ "$status" -ne 0 ]]; then
            echo "Expected zero exit status for functions with timestamps" >&2
            echo "--- output begin ---" >&2
            echo "$output" >&2
            echo "--- output end ---" >&2
            exit 1
        fi
        require_regex "$output" "\\[omega\\] \\[[0-9]+\\.[0-9]{6}\\] \\[app\\] \\[main:[0-9]+:main\\] cli processing enabled, use --trace for options"
        require_not_contains "$output" "CLI error:"
        ;;
    removed_lines_option)
        run_and_split --trace ".app" --trace-lines
        if [[ "$status" -eq 0 ]]; then
            echo "Expected non-zero exit status for removed lines option" >&2
            exit 1
        fi
        require_contains "$output" "[error] [cli] unknown option --trace-lines"
        require_not_contains "$output" "Trace selector examples:"
        ;;
    wildcard_all_depth3)
        run_and_split --trace "*.*.*.*"
        if [[ "$status" -ne 0 ]]; then
            echo "Expected zero exit status for wildcard depth3 selector" >&2
            echo "--- output begin ---" >&2
            echo "$output" >&2
            echo "--- output end ---" >&2
            exit 1
        fi
        require_not_contains "$output" "CLI error:"
        require_regex "$output" "(^|\\n)\\[omega\\] \\[app\\] omega initialized local trace channels($|\\n)"
        require_regex "$output" "(^|\\n)\\[ktrace\\] \\[api\\] processing channels \\(enable api\\.channels for details\\): enabled [0-9]+ channel\\(s\\), 0 unmatched selector\\(s\\)($|\\n)"
        require_contains "$output" "omega trace test on channel 'deep.branch.leaf'"
        require_contains "$output" "[alpha] [net] testing..."
        require_contains "$output" "beta trace test on channel 'io'"
        require_contains "$output" "gamma trace test on channel 'physics'"
        ;;
    brace_selector)
        run_and_split --trace "*.{net,io}"
        if [[ "$status" -ne 0 ]]; then
            echo "Expected zero exit status for brace selector" >&2
            echo "--- output begin ---" >&2
            echo "$output" >&2
            echo "--- output end ---" >&2
            exit 1
        fi
        require_not_contains "$output" "CLI error:"
        require_contains "$output" "[alpha] [net] testing..."
        require_contains "$output" "beta trace test on channel 'io'"
        require_not_contains "$output" "[alpha] [cache] testing..."
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
