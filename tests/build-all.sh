#!/bin/bash
set -euo pipefail

usage() {
    echo "Usage: ./tests/$(basename "$0") [--version <slot>]"
    echo "Defaults to: --version latest"
}

version="latest"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            if [[ $# -lt 2 ]]; then
                echo "Error: --version requires a value" >&2
                usage
                exit 1
            fi
            version="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Error: unknown argument '$1'" >&2
            usage
            exit 1
            ;;
    esac
done

cd "$(cd "$(dirname "$0")/.." && pwd)"

# Build SDK
./kbuild.py -d "build/${version}/"

# Run tests
ctest --test-dir "build/${version}" --output-on-failure

# Build Libraries
cd demo/libraries/alpha/
./abuild.py -a mike -d "./build/${version}/" --ktrace-sdk "../../../build/${version}/sdk/"
cd ../../..

cd demo/libraries/beta/
./abuild.py -a mike -d "./build/${version}/" --ktrace-sdk "../../../build/${version}/sdk/"
cd ../../..

cd demo/libraries/delta/
./abuild.py -a mike -d "./build/${version}/" --ktrace-sdk "../../../build/${version}/sdk/"
cd ../../..

# Build executable
cd demo/executable/
./abuild.py -a mike -d "./build/${version}/" --ktrace-sdk "../../build/${version}/sdk/" \
    --alpha-sdk "../libraries/alpha/build/${version}/sdk/" \
    --beta-sdk "../libraries/beta/build/${version}/sdk/" \
    --delta-sdk "../libraries/delta/build/${version}/sdk/"

exit 0
