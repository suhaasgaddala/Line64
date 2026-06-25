# Line64 Benchmark Summary — capacity 32768 (post PR #9)

Generated from raw JSONL; no numbers hand-entered.

## A. Environment

| Field | Value |
|---|---|
| Git commit | `d1e275a980f1` |
| Build type | Release |
| Compiler | AppleClang 17.0.0.17000404 |
| OS | macOS 26.3.1 (build 25D2128) |
| CPU | Apple M4 (10 cores) |
| Capacity | 32768 |
| Payload size | 64 bytes |
| Duration | 1000 ms |
| Warmup | 250 ms |
| Trials/scenario | 3 |
| Dep: boost | 1.90 (Homebrew, header-only lockfree) |
| Dep: moodycamel::ConcurrentQueue | cameron314/concurrentqueue @ 9afb99746f0f5fc94ac8aef737053ae0481ba8d1 |
| Dep: rigtorp::SPSCQueue | rigtorp/SPSCQueue @ 565a5149d54930463d58cb0f69b978d439555e66 |
| Dep: atomic_queue | max0x7ba/atomic_queue @ c3eeb3d7bf85e8a6414294f5510681b7a595f97c |

## Validation

- Total rows: **87**
- Rows with `validation: passed`: **87**
- Rows with `validation_errors != 0`: **0**
- JSON parse errors: **0**
- Missing groups: **none**
- Missing queues: **none**
- Overall: **ALL CHECKS PASSED**

## B. SPSC exclusive handoff — mean comparison

| Queue | Mean msg/s | Mean bytes/s | Relative to Line64 SPSC | Validation |
|---|--:|--:|--:|---|
| Line64::SPSCQueue | 1,863,926 | 119,291,285 | 1.00x | passed |
| boost::lockfree::spsc_queue | 1,931,582 | 123,621,227 | 1.04x | passed |
| rigtorp::SPSCQueue | 1,940,678 | 124,203,413 | 1.04x | passed |

## C. SPMC multicast retained history — mean comparison

Multicast: every consumer observes every published message. Only Line64 queues with this semantics are compared.

| Queue | Topology | Mean published msg/s | Mean aggregate reads/s | Mean lagged/overwritten | Validation |
|---|---|--:|--:|--:|---|
| Line64::VersionedSPMCQueue | 1P/1C | 1,966,210 | 1,531,633 | 452,694 | passed |
| Line64::VersionedSPMCQueue | 1P/3C | 1,882,912 | 4,477,332 | 1,196,579 | passed |
| Line64::VersionedSPMCQueue | 1P/10C | 1,186,448 | 10,770,087 | 629,496 | passed |
| Line64::SPMCMulticastQueue | 1P/1C | 1,726,377 | 1,593,824 | 86,499 | passed |
| Line64::SPMCMulticastQueue | 1P/3C | 1,549,005 | 4,646,872 | 8 | passed |
| Line64::SPMCMulticastQueue | 1P/10C | 249,124 | 2,474,913 | 1,788 | passed |

## D. MPMC work sharing — mean comparison

Exclusive-pop: each message consumed once. Ranked within each topology.

### 1P/1C

| Queue | Mean msg/s | Mean bytes/s | Rank | Relative to Line64 MPMC | Validation |
|---|--:|--:|--:|--:|---|
| atomic_queue::AtomicQueueB2 | 1,889,764 | 120,944,917 | 1 | 1.00x | passed |
| Line64::MPMCQueue | 1,882,846 | 120,502,165 | 2 | 1.00x | passed |
| moodycamel::ConcurrentQueue | 1,805,396 | 115,545,323 | 3 | 0.96x | passed |
| boost::lockfree::queue | 1,785,237 | 114,255,189 | 4 | 0.95x | passed |
| Line64::BlockingQueue | 1,742,240 | 111,503,360 | 5 | 0.93x | passed |

### 2P/2C

| Queue | Mean msg/s | Mean bytes/s | Rank | Relative to Line64 MPMC | Validation |
|---|--:|--:|--:|--:|---|
| atomic_queue::AtomicQueueB2 | 3,589,929 | 229,755,477 | 1 | 1.07x | passed |
| Line64::MPMCQueue | 3,363,076 | 215,236,885 | 2 | 1.00x | passed |
| moodycamel::ConcurrentQueue | 3,346,192 | 214,156,309 | 3 | 0.99x | passed |
| boost::lockfree::queue | 3,219,304 | 206,035,435 | 4 | 0.96x | passed |
| Line64::BlockingQueue | 2,428,053 | 155,395,371 | 5 | 0.72x | passed |

### 4P/4C

| Queue | Mean msg/s | Mean bytes/s | Rank | Relative to Line64 MPMC | Validation |
|---|--:|--:|--:|--:|---|
| atomic_queue::AtomicQueueB2 | 2,359,185 | 150,987,861 | 1 | 1.03x | passed |
| moodycamel::ConcurrentQueue | 2,331,844 | 149,237,995 | 2 | 1.02x | passed |
| Line64::MPMCQueue | 2,280,157 | 145,930,069 | 3 | 1.00x | passed |
| boost::lockfree::queue | 2,057,698 | 131,692,651 | 4 | 0.90x | passed |
| Line64::BlockingQueue | 852,846 | 54,582,165 | 5 | 0.37x | passed |

### 8P/8C

| Queue | Mean msg/s | Mean bytes/s | Rank | Relative to Line64 MPMC | Validation |
|---|--:|--:|--:|--:|---|
| boost::lockfree::queue | 1,946,148 | 124,553,451 | 1 | 1.03x | passed |
| moodycamel::ConcurrentQueue | 1,907,506 | 122,080,384 | 2 | 1.01x | passed |
| Line64::MPMCQueue | 1,896,813 | 121,396,011 | 3 | 1.00x | passed |
| atomic_queue::AtomicQueueB2 | 1,816,188 | 116,236,032 | 4 | 0.96x | passed |
| Line64::BlockingQueue | 768,794 | 49,202,837 | 5 | 0.41x | passed |

## E. Wins (defensible only)

- Line64::SPSCQueue did NOT rank #1 (winner: rigtorp::SPSCQueue).
- VersionedSPMCQueue beats SPMCMulticastQueue at 1P/1C (1,966,210 vs 1,726,377 published msg/s, 1.14x).
- VersionedSPMCQueue beats SPMCMulticastQueue at 1P/3C (1,882,912 vs 1,549,005 published msg/s, 1.22x).
- VersionedSPMCQueue beats SPMCMulticastQueue at 1P/10C (1,186,448 vs 249,124 published msg/s, 4.76x).

## F. Caveats

- Capacity is 32768 because Boost.Lockfree MPMC (boost::lockfree::queue) rejects 65536 with its freelist index limit.
- Results are specific to this local machine (Apple M4, macOS) and this single run; absolute numbers will differ elsewhere.
- SPMC multicast aggregate reads are NOT comparable to SPSC/MPMC exclusive-pop throughput; every consumer sees every message.
- SPMC is reported in its own table and only VersionedSPMCQueue vs SPMCMulticastQueue are compared.
