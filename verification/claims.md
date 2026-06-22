# Verification Claims Matrix

This matrix ties each checked property to an executable artifact. Model checking
uses finite configurations and does not establish an unrestricted proof of the
C++ implementation.

| Queue | Property | Executable evidence | Implementation evidence |
| --- | --- | --- | --- |
| `BlockingQueue<T>` | Bounded FIFO conservation, unique acceptance/consumption, close prevents later pushes | `tla/BlockingQueue.tla` | Unit tests, close/wakeup tests, TSan/ASan/UBSan configurations |
| `SPSCQueue<N>` | Capacity bound, FIFO sequences, no reuse before consumer release | `tla/SPSCQueue.tla` | `genmc/spsc_core.c` protocol model, boundary/concurrency tests, seeded stress, TSan/ASan/UBSan configurations |
| `SPMCMulticastQueue<N>` | Registration boundary, ordered per-consumer reads, retained-slot integrity, lag recovery | `tla/SPMCMulticastQueue.tla` | `genmc/spmc_mutex_core.c` protocol model, multicast/lag tests, seeded stress, TSan/ASan/UBSan configurations |
| `MPMCQueue<N>` | Unique position claims, publish-before-pop, no duplicate pops, payload/position matching, capacity-two cell reuse | `tla/MPMCQueue.tla` | `genmc/mpmc_core.c` protocol model, 50,000-message contention test, seeded stress, sanitizer configurations, documented sequence-cell protocol |

## Model Bounds

- Blocking: capacity 2, four distinct values, arbitrary push/pop/close ordering.
- SPSC: capacity 2, four publications, producer write/publish and consumer
  copy/release split into independently scheduled steps.
- SPMC: capacity 2, two independently registered consumers, four publications,
  arbitrary registration/read/lag scheduling.
- MPMC: capacity 2, four push operations and four pop operations, with claim,
  write, publish, claim, and release scheduled independently.

## Claims Not Established

- correctness after position or logical-sequence exhaustion;
- an unbounded refinement proof from the TLA+ models to the C++ implementation;
- lock-free or wait-free progress;
- behavior outside each queue's documented producer/consumer contract;
- safe destruction while operations or consumer handles remain active;
- production readiness on platforms not covered by the build and sanitizer
  matrix.
