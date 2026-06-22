# Concurrency Verification

This directory contains executable finite-state models for the four public queue
contracts. The models are maintained alongside the implementation and may
evolve with it; they do not freeze the API.

## Run TLC

Requirements:

- Java 11 or newer;
- TLA+ tools `v1.7.4` (`tla2tools.jar`, SHA-256
  `936a262061c914694dfd669a543be24573c45d5aa0ff20a8b96b23d01e050e88`).

Run:

```sh
TLA2TOOLS_JAR=/path/to/tla2tools.jar \
JAVA_BIN=/path/to/java \
./verification/run_tlc.sh
```

Tool output and state databases are written below `build-verification/`, which
is ignored by Git.

## Scope

The models deliberately use small capacities and operation counts so TLC can
enumerate every reachable state in those configurations. The MPMC model splits
operations at the algorithm's ownership transitions, allowing producers and
consumers to stall after claiming positions and before publishing or releasing
cells.

Passing TLC means no configured invariant was violated in the complete finite
state space. It is stronger than randomized testing for that model and bound,
but it is not a proof that the C++ implementation refines the model for all
capacities, counters, platforms, and executions.

See [claims.md](claims.md) for checked properties and explicit non-claims, and
[results.md](results.md) for the latest recorded execution results.

## Run GenMC

The protocol harnesses mirror the atomic orders and ownership transitions used
by the SPSC and MPMC headers, plus the mutex exclusion used by multicast. They
run under GenMC's weak-memory execution explorer through a pinned container
image:

```sh
./verification/run_genmc.sh
```

This requires a running Docker daemon. The pinned image contains GenMC v0.16.1
built with LLVM 18.1.8. On ARM64 hosts Docker uses x86-64 emulation because the
upstream image currently targets `linux/amd64`.

These are reduced protocol models rather than compiled instances of the public
C++ headers. GenMC's reduced runtime cannot currently process the libstdc++
allocation and memory intrinsics used by those headers, and its pthread shim
does not implement condition variables. Actual-header evidence therefore comes
from the unit, stress, ASan/UBSan, and TSan configurations.

Negative controls replace the acquire/release handoffs with relaxed operations
in generated copies and require GenMC to report a non-atomic payload race:

```sh
./verification/run_negative_controls.sh
```
