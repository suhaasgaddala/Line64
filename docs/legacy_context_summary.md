# Legacy Context Summary

The original OrbitQueue was an educational C++20 prototype exploring
fixed-size in-memory queues. It had no automated tests or CI, hard-coded build
assumptions, conflicting global types, unclear multicast SPMC semantics, and
unsafe APIs that exposed raw writes and caller-managed ring indices. Its
benchmark also compared queues with different delivery semantics and payloads.

OrbitQueue v2 starts from explicit contracts, bounded span-based payload APIs,
portable CMake, correctness tests, sanitizer support, and semantically honest
benchmarks. The legacy implementation remains separate and is not used as the
v2 algorithm foundation.
