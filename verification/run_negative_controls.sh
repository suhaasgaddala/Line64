#!/bin/sh

set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_directory="$repository_root/build-verification/negative-controls"
image=${GENMC_IMAGE:-genmc/genmc@sha256:25fe5f67a399c095534eefafa8cf9401637a7750bee5a93496896025c89a6353}
binary=/usr/local/bin/genmc/genmc

if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
    echo "A running Docker daemon is required for GenMC negative controls." >&2
    exit 2
fi

mkdir -p "$output_directory"

for harness in spsc_core mpmc_core; do
    generated="$output_directory/${harness}_relaxed.c"
    log="$output_directory/${harness}_relaxed.log"
    sed \
        -e 's/memory_order_release/memory_order_relaxed/g' \
        -e 's/memory_order_acquire/memory_order_relaxed/g' \
        "$repository_root/verification/genmc/$harness.c" > "$generated"

    echo "==> GenMC negative control: $harness"
    set +e
    docker run --rm --platform linux/amd64 \
        --volume "$repository_root:/workspace:ro" \
        --workdir /tmp \
        "$image" \
        "$binary" --disable-estimation --v1 -- \
        -std=c11 \
        "/workspace/build-verification/negative-controls/${harness}_relaxed.c" \
        > "$log" 2>&1
    exit_code=$?
    set -e

    if [ "$exit_code" -eq 0 ]; then
        echo "Negative control unexpectedly passed: $harness" >&2
        cat "$log" >&2
        exit 1
    fi
    if ! grep -q "Non-atomic race" "$log"; then
        echo "Negative control failed for an unexpected reason: $harness" >&2
        cat "$log" >&2
        exit 1
    fi
    echo "expected non-atomic race detected"
done
