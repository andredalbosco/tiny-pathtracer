#!/usr/bin/env bash
set -e
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

exec watchexec --restart --debounce 250ms --stop-timeout 2s \
    --watch-non-recursive . --watch-non-recursive experiments \
    --filter '**/cloud_viewer.cpp' --filter '**/Makefile' \
    --shell=none -- bash -c \
    'make cloud-viewer compdb && exec ./build/cloud_viewer "$@"' bash "$@"
