#!/bin/sh

set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
image=${GENMC_IMAGE:-genmc/genmc@sha256:25fe5f67a399c095534eefafa8cf9401637a7750bee5a93496896025c89a6353}
binary=/usr/local/bin/genmc/genmc

if ! command -v docker >/dev/null 2>&1; then
    echo "Docker is required to run the pinned GenMC container." >&2
    exit 2
fi

if ! docker info >/dev/null 2>&1; then
    echo "Docker is installed but its daemon is not running." >&2
    exit 2
fi

for harness in spsc_core spmc_mutex_core mpmc_core; do
    echo "==> GenMC: $harness"
    docker run --rm --platform linux/amd64 \
        --volume "$repository_root:/workspace:ro" \
        --workdir /tmp \
        "$image" \
        "$binary" --disable-estimation --v2 -- \
        -std=c11 \
        "/workspace/verification/genmc/$harness.c"
done
