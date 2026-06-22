# Benchmarking

The benchmark measures SPSC work sharing and SPMC multicast in separate runs.
It emits one JSON object per line containing queue name, capacity, payload
size, producer and consumer counts, requested duration, publications,
aggregate reads, verified unique sequences, and lag events.

`messages_published` counts accepted producer operations. `aggregate_reads`
counts all successful consumer reads. `unique_sequences_verified` is the union
of sequences whose payload matched its sequence. In multicast, one publication
can contribute several aggregate reads, so aggregate reads are not comparable
to unique SPSC pops. `dropped_or_lagged` counts detected cursor recovery events,
not the exact number of messages skipped.

The legacy benchmark compared different payloads and different delivery
semantics, lacked correctness checks, and did not retain machine or trial
metadata. Its totals therefore were not a fair queue ranking.

The included executable is a small reproducible smoke benchmark, not a full
performance study. It has no warmup, affinity control, latency distribution,
or statistical repetitions. Throughput is not correctness; tests and
sanitizers are separate gates that must pass before performance interpretation.
