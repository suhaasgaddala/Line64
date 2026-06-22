# Research Motivation

Bounded Concurrent Queues for C++20 explores fixed-size, in-memory queue
designs with explicit ownership, delivery, overflow, ordering, and shutdown
contracts. The work was inspired in part by the CppCon 2022 talk
[*Trading at Light Speed*](https://youtu.be/8uAW5FQtcvE) and by the question of
whether localized per-slot metadata can reduce contention relative to global
queue indices.

Per-slot metadata alone is not a correctness argument. A producer can still
rewrite non-atomic payload bytes while a consumer copies them, and independent
consumers can overwrite one another's metadata updates. Acquire/release
ordering publishes prior writes but does not prevent a future writer from
racing with an in-progress reader.

The current library therefore starts from explicit contracts:

- `SPSCQueue<N>` uses single-writer ownership of head and tail positions.
- `SPMCMulticastQueue<N>` uses independent consumer cursors and a mutex across
  publication and payload copy.
- `MPMCQueue<N>` uses sequence-numbered ring cells and CAS position claims.
- `BlockingQueue<T>` provides a conventional bounded mutex/condition-variable
  baseline with close and drain behavior.

The benchmark harness follows the same discipline. It uses comparable
sequence-bearing payloads, distinguishes multicast observations from unique
work-sharing pops, validates payload integrity, records retry/error counters,
and keeps Boost.Lockfree optional and disabled by default.

Technical alternatives and their disposition are documented in
[design_decisions.md](design_decisions.md). Detailed exploratory mechanisms and
failure analysis are recorded in
[design_explorations.md](design_explorations.md).
