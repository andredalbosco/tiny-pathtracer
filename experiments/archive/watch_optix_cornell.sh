#!/usr/bin/env bash
set -u
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
trap 'exit 0' INT TERM

snapshot() {
    sha256sum pathtracer_optix.{cpp,cu,h} external/cuda/helper_math.h Makefile
}

render() {
    printf '\nBuilding and rendering...\n'
    make optix-clang compdb && ./build/pathtracer_optix_clang
}

previous=$(snapshot)
render
printf '\nWatching OptiX sources and Makefile. Ctrl+C to stop.\n'
while sleep 0.5; do
    current=$(snapshot)
    if [[ "$current" != "$previous" ]]; then
        previous=$current
        render
    fi
done
