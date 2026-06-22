# Verification Results

Validation date: 2026-06-22.

These results apply to the source and verification artifacts committed together
in this milestone. Generated state databases and tool runtimes are intentionally
excluded from Git.

## TLC v1.7.4

Every configured state space completed with zero invariant violations and zero
states left unexplored.

| Model | Generated states | Distinct states | Graph depth |
| --- | ---: | ---: | ---: |
| Blocking queue | 562 | 378 | 10 |
| SPSC queue | 41 | 29 | 17 |
| Multicast SPMC queue | 2,002 | 1,289 | 15 |
| Sequence-cell MPMC queue | 24,705 | 14,265 | 21 |

TLC action coverage confirmed that push, pop, close, producer write/publish,
consumer copy/release, registration, lag recovery, MPMC position claims, cell
publication, and cell release were all reached.

## GenMC v0.16.1

The pinned GenMC container was run under `linux/amd64` emulation on an ARM64
host using the RC11 default memory model.

| Protocol harness | Complete executions | Blocked executions | Result |
| --- | ---: | ---: | --- |
| SPSC acquire/release handoff | 1 | 0 | No errors detected |
| SPMC mutex exclusion | 2 | 2 | No errors detected |
| MPMC sequence cells, capacity 2, three pushes/pops | 36 | 3,185 | No errors detected |

The GenMC programs are reduced protocol mirrors. They use the same atomic orders
and ownership transitions as the headers but are not a refinement proof of the
compiled C++ templates.

Generated SPSC and MPMC negative controls weakened all acquire/release handoffs
to relaxed operations. GenMC rejected both with the expected non-atomic payload
race, confirming that the harness assertions are sensitive to the synchronization
being checked.

## Actual C++ Implementation

Fresh Apple Clang 17 builds on ARM64 macOS completed:

- Debug CTest: 7/7 passed;
- Release CTest: 7/7 passed;
- ASan/UBSan CTest with benchmarks disabled: 6/6 passed;
- TSan CTest with benchmarks disabled: 6/6 passed;
- five Release all-queue stress seeds at capacity 8 and 10,000 iterations:
  passed with zero validation failures;
- each Release MPMC run pushed and popped 40,000 messages without loss;
- full ASan/UBSan all-queue stress: passed with no report;
- full TSan all-queue stress: passed with no report.

## Interpretation

This milestone adds exhaustive finite-state evidence for the configured models,
weak-memory checking for reduced synchronization protocols, and dynamic
sanitizer/stress evidence for the actual implementation. It does not establish
an unbounded implementation-refinement proof, progress guarantees, counter-wrap
correctness, unsupported ownership patterns, or safe concurrent destruction.
