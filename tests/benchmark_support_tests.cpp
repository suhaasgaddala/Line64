#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "benchmark_support.h"
#include "test_support.h"

using orbitqueue::benchmark::Result;
using orbitqueue::benchmark::SequenceTracker;
using test_support::expect;

void run_benchmark_support_tests() {
    constexpr std::array<std::size_t, 3> expected_consumers{1, 3, 10};
    static_assert(
        orbitqueue::benchmark::default_consumer_matrix == expected_consumers);

    std::vector<SequenceTracker> trackers(2);
    expect(trackers[0].record(1) && trackers[0].record(2) && trackers[0].record(5),
           "sequence tracker must accept increasing sequences and gaps");
    expect(trackers[1].record(2) && trackers[1].record(3) && trackers[1].record(4),
           "independent sequence tracker must accept overlapping ranges");
    expect(!trackers[1].record(4) && !trackers[1].record(0),
           "sequence tracker must reject duplicates and sequence zero");
    expect(orbitqueue::benchmark::count_unique_sequences(trackers) == 5,
           "sequence range union must count unique values across consumers");

    Result result;
    result.benchmark_group = "example_group";
    result.queue = "example\nqueue";
    result.delivery_semantics = "exclusive_pop";
    result.trial = 2;
    result.capacity = 16;
    result.payload_size = 32;
    result.producer_count = 1;
    result.consumer_count = 3;
    result.duration_ms = 25;
    result.warmup_ms = 5;
    result.messages_published = 100;
    result.aggregate_reads = 90;
    result.unique_sequences_verified = 80;
    result.dropped_or_lagged = 2;
    result.invalid_payloads = 1;
    result.full_retries = 7;
    result.empty_retries = 9;
    result.validation_errors = 3;
    result.throughput_messages_per_second = 4000.0;
    result.throughput_reads_per_second = 3600.0;
    result.throughput_bytes_per_second = 115200.0;
    result.consumer_reads = {30, 31, 29};
    result.consumer_reads_per_second = {1200.0, 1240.0, 1160.0};
    result.build_type = "Release";
    result.compiler = "Example 1.0";
    result.git_commit = "abc123";
    result.timestamp = "2026-06-22T00:00:00Z";
    result.notes = "quoted: \"value\"";
    const std::string expected =
        "{\"benchmark_group\":\"example_group\","
        "\"queue\":\"example\\nqueue\","
        "\"delivery_semantics\":\"exclusive_pop\","
        "\"trial\":2,\"capacity\":16,"
        "\"payload_size\":32,\"payload_bytes\":32,"
        "\"producer_count\":1,\"producers\":1,"
        "\"consumer_count\":3,\"consumers\":3,"
        "\"duration_ms\":25,\"duration_seconds\":0.025,\"warmup_ms\":5,"
        "\"messages_published\":100,\"messages_produced\":100,"
        "\"aggregate_reads\":90,\"messages_consumed\":90,"
        "\"unique_sequences_verified\":80,\"dropped_or_lagged\":2,"
        "\"invalid_payloads\":1,\"full_retries\":7,"
        "\"producer_failed_tries\":7,\"empty_retries\":9,"
        "\"consumer_failed_tries\":9,\"consumer_lagged_count\":2,"
        "\"validation_errors\":3,\"validation\":\"failed\","
        "\"throughput_messages_per_second\":4000,"
        "\"messages_per_second\":4000,"
        "\"throughput_reads_per_second\":3600,"
        "\"bytes_per_second\":115200,"
        "\"consumer_reads\":[30,31,29],"
        "\"consumer_reads_per_second\":[1200,1240,1160],"
        "\"build_type\":\"Release\","
        "\"compiler\":\"Example 1.0\",\"git_commit\":\"abc123\","
        "\"timestamp\":\"2026-06-22T00:00:00Z\","
        "\"notes\":\"quoted: \\\"value\\\"\"}";
    expect(orbitqueue::benchmark::to_json(result) == expected,
           "benchmark result JSON schema must remain stable");
}
