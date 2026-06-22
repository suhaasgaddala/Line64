# OrbitQueue v2

OrbitQueue is a C++20 concurrency research library for fixed-size in-memory
queues. It focuses on explicit delivery semantics, correctness testing, and
reproducible benchmarking.

OrbitQueue v2 is an experimental foundation for studying bounded queue
contracts. It is not a production-readiness claim, a formal verification, or
evidence that any implementation is the fastest available.

## Queue types

- `BlockingQueue<T>`: bounded mutex-based FIFO with close and drain behavior.
- `SPSCQueue<N>`: bounded single-producer, single-consumer work-sharing FIFO
  with fixed-size payloads and no overwrite.
- `SPMCMulticastQueue<N>`: single-producer multicast ring where each registered
  consumer has an independent cursor. A mutex protects publication and reads;
  slow consumers detect overwritten history.

The public payload APIs use `std::span` and reject oversized messages. Queue
status and sequence information are returned explicitly.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

CMake 3.20 and a C++20 compiler are required. The core library has no mandatory
third-party dependencies.

Useful options:

```sh
-DORBITQUEUE_BUILD_TESTS=ON
-DORBITQUEUE_BUILD_BENCHMARKS=ON
-DORBITQUEUE_ENABLE_WARNINGS=ON
-DORBITQUEUE_ENABLE_SANITIZERS=ON
-DORBITQUEUE_SANITIZERS=address,undefined
```

For ThreadSanitizer, use a separate build with
`-DORBITQUEUE_ENABLE_SANITIZERS=ON -DORBITQUEUE_SANITIZERS=thread` because TSan
cannot generally be combined with ASan.

## Benchmark

```sh
./build/benchmarks/orbitqueue_benchmark 1000
```

The optional argument is duration in milliseconds. Each queue emits one JSON
line. SPSC unique pops and multicast aggregate reads have different meanings
and must not be ranked as equivalent work. See [docs/benchmarking.md](docs/benchmarking.md).

## Correctness status

Tests cover boundaries, FIFO order, wraparound, close wakeups, concurrent SPSC
sequence integrity, independent multicast cursors, and lag detection. The SPSC
implementation uses acquire/release atomics under its stated one-producer,
one-consumer contract. The multicast implementation deliberately uses a mutex
to avoid payload races during overwrite. No lock-free progress guarantee is
made.

## Roadmap

- Expand randomized and long-duration sanitizer testing.
- Add package installation and downstream-consumer tests.
- Record benchmark environment metadata and repeated-trial statistics.
- Investigate faster multicast synchronization only behind equivalent
  correctness tests.

Detailed contracts are in [docs/queue_contracts.md](docs/queue_contracts.md).
