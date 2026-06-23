# Benchmarking

`orbitqueue_benchmark` is a dependency-free, contract-aware throughput harness
by default. It validates every payload while measuring and emits one JSON object
per trial. Optional third-party baselines can be enabled explicitly, but they do
not affect the default build.

## Build and run

Use a Release build for performance work:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
./build-release/benchmarks/orbitqueue_benchmark \
  --duration-ms 1000 --warmup-ms 100 --trials 5
```

Available options are:

```text
--duration-ms <uint64>
--warmup-ms <uint64>
--trials <uint32>
--capacity <uint32>
--payload-size <uint32>
--producers <uint32>
--consumers <default|single|comma-separated counts>
--queue <all|spsc|blocking|spmc|mpmc|boost|boost_spsc|boost_mpmc|moodycamel_mpmc|rigtorp_spsc|atomic_queue_mpmc>
--help
```

Payload size must be between 32 and 256 bytes because every message carries a
32-byte validation header. The general default SPMC consumer preset is
`1,3,10`. MPMC exclusive-pop baselines use the `1/1`, `2/2`, `4/4`, `8/8`
matrix unless producers or consumers are explicitly configured. Selecting one
queue applies configured values where its contract permits them. MPMC scenarios
require a power-of-two capacity greater than one.

## Benchmarking by delivery semantics

Benchmarks are grouped by delivery semantics before throughput is compared.
SPSC queues are compared only against SPSC baselines. MPMC queues are compared
only against exclusive-pop MPMC baselines. SPMC multicast is reported
separately because aggregate consumer observations are not directly comparable
to exclusive-pop throughput.

### SPSC: exclusive handoff

| Baseline | Queue selector | Status |
| --- | --- | --- |
| Line64 `SPSCQueue` | `spsc` | Implemented |
| `boost::lockfree::spsc_queue` | `boost_spsc` or `boost` | Optional, disabled by default |
| `rigtorp/SPSCQueue` | `rigtorp_spsc` | Optional, disabled by default |
| `folly::ProducerConsumerQueue` | none | Future work only |

Metrics: messages/sec, bytes/sec, failed push/pop attempts, and checksum
validation. The topology is always one producer and one consumer.

### MPMC: exclusive-pop work-sharing

| Baseline | Queue selector | Status |
| --- | --- | --- |
| Line64 `MPMCQueue` | `mpmc` | Implemented |
| Line64 `BlockingQueue` | `blocking` | Implemented |
| `boost::lockfree::queue` | `boost_mpmc` or `boost` | Optional, disabled by default |
| `moodycamel::ConcurrentQueue` | `moodycamel_mpmc` | Optional, disabled by default |
| `max0x7ba/atomic_queue` | `atomic_queue_mpmc` | Optional, disabled by default |

Metrics: messages/sec, bytes/sec, producer failed tries, consumer failed tries,
and checksum validation. The default topology matrix is `1/1`, `2/2`, `4/4`,
and `8/8`. Optional stress topologies such as `1/8` or `8/1` can be requested
with `--producers` and `--consumers`.

### SPMC: multicast retained history

| Baseline | Queue selector | Status |
| --- | --- | --- |
| Line64 `SPMCMulticastQueue` | `spmc` | Implemented |

Metrics: published messages/sec, aggregate consumer observations/sec,
per-consumer observations/sec, `consumer_lagged` count, and retained history /
overwrite behavior. SPMC multicast results are not directly comparable to
exclusive-pop MPMC or SPSC throughput.

The default `--queue all` run includes Line64 SPSC, Line64 SPMC multicast,
Line64 `BlockingQueue`, Line64 `MPMCQueue`, and any optional baselines compiled
into the executable.

## Warmup and trials

Each scenario receives a separate warmup run before measured trials. Warmup
results are discarded, but the executable exits unsuccessfully if warmup
validation fails. `--warmup-ms 0` disables warmup.

Trials create a fresh queue and fresh threads. One JSON line is emitted per
measured trial. The harness deliberately reports raw trials instead of averaging
them in-process so callers can retain samples, choose statistics, and identify
outliers without losing information.

## Output schema

Every JSON object contains:

| Field | Meaning |
| --- | --- |
| `benchmark_group` | One of `spsc_exclusive_handoff`, `mpmc_work_sharing`, or `spmc_multicast_retained_history` |
| `queue`, `trial` | Scenario identity and one-based measured trial number |
| `delivery_semantics` | `exclusive_handoff`, `exclusive_pop`, or `multicast_retained_history` |
| `capacity`, `payload_size`, `payload_bytes` | Queue and message configuration; `payload_bytes` is a stable alias for `payload_size` |
| `producer_count`, `consumer_count`, `producers`, `consumers` | Active worker counts |
| `duration_ms`, `duration_seconds`, `warmup_ms` | Requested measured and warmup intervals |
| `messages_published`, `messages_produced` | Successful producer operations |
| `aggregate_reads`, `messages_consumed` | Successful reads summed across consumers |
| `unique_sequences_verified` | Distinct validated payload IDs observed |
| `dropped_or_lagged` | SPMC cursor-recovery events; not an exact skipped-message count |
| `invalid_payloads` | Checksum, header, or generated-byte mismatches |
| `full_retries`, `empty_retries` | Non-blocking boundary results retried by workers |
| `producer_failed_tries`, `consumer_failed_tries` | Stable aliases for push-full and pop-empty retry counters |
| `consumer_lagged_count` | Stable alias for retained-history lag or overwrite events |
| `validation_errors`, `validation` | All detected correctness failures and a `passed` / `failed` summary |
| `throughput_messages_per_second` | Publications divided by requested measured seconds |
| `messages_per_second` | Stable alias for `throughput_messages_per_second` |
| `throughput_reads_per_second` | Aggregate reads divided by requested measured seconds |
| `bytes_per_second` | Validated read bytes divided by requested measured seconds |
| `consumer_reads`, `consumer_reads_per_second` | Per-consumer successful reads and per-consumer observation rates |
| `build_type`, `compiler`, `git_commit`, `timestamp` | Build and run provenance, or `unknown` where unavailable |
| `notes` | Delivery-semantics caveat for the scenario |

Work-sharing scenarios require exactly one validated read for every successful
publication after drain. SPMC readers may lag and may each observe the same
publication, so its aggregate reads are neither unique pops nor directly
comparable to work-sharing throughput. Any validation error makes the process
exit nonzero.

Payload validation and ID retention add measurement overhead. The harness stores
observed IDs during each trial to detect duplicates and missing work; memory use
therefore grows with successful reads. Scheduling, CPU frequency, topology,
affinity, background load, allocator behavior, and compiler flags can all affect
results. The metadata is provenance, not full environment capture, and the
harness does not report latency distributions or statistical significance.

## Optional external baselines

All external baselines are benchmark-only and default OFF. The default build
does not fetch or require third-party queue libraries.

```sh
cmake -S . -B build-baselines -DCMAKE_BUILD_TYPE=Release \
  -DLINE64_ENABLE_BOOST_BENCHMARKS=ON \
  -DLINE64_ENABLE_MOODYCAMEL_BENCHMARKS=ON \
  -DLINE64_ENABLE_RIGTORP_BENCHMARKS=ON \
  -DLINE64_ENABLE_ATOMIC_QUEUE_BENCHMARKS=ON
```

The compatible `ORBITQUEUE_ENABLE_*` option names are retained. Boost is
discovered from system headers. moodycamel, rigtorp, and atomic_queue are
header-only FetchContent dependencies pinned to specific upstream commits and
are fetched only when explicitly enabled. Missing Boost headers warn and omit
only Boost scenarios. FetchContent failures affect only the explicitly enabled
optional baseline build.

The word `lockfree` in Boost type names identifies upstream library names; this
project does not infer or claim progress guarantees for third-party baselines.
