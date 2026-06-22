#!/bin/sh

set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
model_directory="$repository_root/verification/tla"
output_root="$repository_root/build-verification/tlc"

default_jar="$repository_root/build-verification/tools/tla/tla2tools.jar"
tla2tools_jar=${TLA2TOOLS_JAR:-$default_jar}

if [ -n "${JAVA_BIN:-}" ]; then
    java_bin=$JAVA_BIN
elif [ -x "$repository_root/build-verification/tools/jre/Contents/Home/bin/java" ]; then
    java_bin="$repository_root/build-verification/tools/jre/Contents/Home/bin/java"
elif command -v java >/dev/null 2>&1; then
    java_bin=$(command -v java)
else
    echo "Java 11 or newer is required; set JAVA_BIN to its executable." >&2
    exit 2
fi

if [ ! -f "$tla2tools_jar" ]; then
    echo "TLA+ tools not found at $tla2tools_jar" >&2
    echo "Set TLA2TOOLS_JAR to a pinned tla2tools.jar release." >&2
    exit 2
fi

mkdir -p "$output_root"

for model in BlockingQueue SPSCQueue SPMCMulticastQueue MPMCQueue; do
    echo "==> TLC: $model"
    rm -rf "$output_root/$model"
    mkdir -p "$output_root/$model"
    (
        cd "$model_directory"
        "$java_bin" -XX:+UseParallelGC -cp "$tla2tools_jar" tlc2.TLC \
            -cleanup \
            -workers auto \
            -coverage 1 \
            -metadir "$output_root/$model" \
            -config "$model.cfg" \
            "$model.tla"
    )
done
