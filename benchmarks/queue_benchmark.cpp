#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(ORBITQUEUE_HAVE_BOOST_LOCKFREE)
#include <boost/lockfree/queue.hpp>
#endif

#include "benchmark_support.h"
#include "orbitqueue/blocking_queue.h"
#include "orbitqueue/mpmc_queue.h"
#include "orbitqueue/spmc_multicast_queue.h"
#include "orbitqueue/spsc_queue.h"

#if !defined(ORBITQUEUE_BENCHMARK_BUILD_TYPE)
#define ORBITQUEUE_BENCHMARK_BUILD_TYPE "unknown"
#endif
#if !defined(ORBITQUEUE_BENCHMARK_COMPILER)
#define ORBITQUEUE_BENCHMARK_COMPILER "unknown"
#endif
#if !defined(ORBITQUEUE_BENCHMARK_GIT_COMMIT)
#define ORBITQUEUE_BENCHMARK_GIT_COMMIT "unknown"
#endif

namespace {

using Clock = std::chrono::steady_clock;
using orbitqueue::QueueStatus;
using orbitqueue::benchmark::Result;
using orbitqueue::benchmark::SequenceTracker;

constexpr std::size_t max_payload_size = 256;
constexpr std::size_t payload_header_size = 32;

struct Config {
    std::uint64_t duration_ms{250};
    std::uint64_t warmup_ms{50};
    std::uint32_t trials{3};
    std::uint32_t capacity{
        static_cast<std::uint32_t>(orbitqueue::benchmark::default_capacity)};
    std::uint32_t payload_size{64};
    std::uint32_t producers{1};
    std::vector<std::size_t> consumers{
        orbitqueue::benchmark::default_consumer_matrix.begin(),
        orbitqueue::benchmark::default_consumer_matrix.end()};
    std::string queue{"all"};
    bool custom_producers{};
    bool custom_consumers{};
};

struct PayloadHeader {
    std::uint64_t global_id{};
    std::uint64_t local_sequence{};
    std::uint32_t producer_id{};
    std::uint32_t payload_size{};
    std::uint64_t checksum{};
};

static_assert(sizeof(PayloadHeader) == payload_header_size);

struct Payload {
    std::array<std::byte, max_payload_size> bytes{};
    std::uint32_t size{};
};

enum class ScenarioKind {
    spsc,
    blocking,
    spmc,
    mpmc,
    boost
};

struct Scenario {
    ScenarioKind kind{};
    std::string name;
    std::size_t producers{};
    std::size_t consumers{};
    std::string notes;
};

struct ConsumerMetrics {
    std::uint64_t reads{};
    std::uint64_t lag_events{};
    std::uint64_t empty_retries{};
    std::uint64_t invalid_payloads{};
    std::uint64_t validation_errors{};
    std::vector<std::uint64_t> payload_ids;
    SequenceTracker queue_sequences;
};

struct Metadata {
    std::string build_type;
    std::string compiler;
    std::string git_commit;
    std::string timestamp;
};

[[nodiscard]] std::uint64_t mix(const PayloadHeader& header) noexcept {
    auto value = header.global_id ^
                 (static_cast<std::uint64_t>(header.producer_id) << 32U) ^
                 header.local_sequence ^ 0x4f52424954515545ULL;
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] Payload make_payload(
    const std::uint32_t payload_size,
    const std::uint32_t producer_id,
    const std::uint64_t local_sequence,
    const std::uint64_t global_id) {
    Payload payload;
    payload.size = payload_size;
    PayloadHeader header{
        global_id, local_sequence, producer_id, payload_size, 0};
    header.checksum = mix(header);
    std::memcpy(payload.bytes.data(), &header, sizeof(header));
    std::mt19937_64 random(header.checksum);
    for (std::size_t index = sizeof(header); index < payload.size; ++index) {
        payload.bytes[index] = static_cast<std::byte>(random() & 0xffU);
    }
    return payload;
}

[[nodiscard]] std::span<const std::byte> readable(const Payload& payload) noexcept {
    return {payload.bytes.data(), payload.size};
}

[[nodiscard]] std::span<std::byte> writable(Payload& payload) noexcept {
    return {payload.bytes.data(), payload.size};
}

[[nodiscard]] PayloadHeader read_header(const Payload& payload) noexcept {
    PayloadHeader header;
    if (payload.size >= sizeof(header)) {
        std::memcpy(&header, payload.bytes.data(), sizeof(header));
    }
    return header;
}

[[nodiscard]] bool valid_payload(
    const Payload& payload,
    const std::uint32_t expected_size) {
    if (payload.size != expected_size || payload.size < sizeof(PayloadHeader)) {
        return false;
    }
    const auto header = read_header(payload);
    if (header.payload_size != payload.size || header.global_id == 0 ||
        header.local_sequence == 0 || header.checksum != mix(header)) {
        return false;
    }
    const auto expected = make_payload(
        payload.size, header.producer_id, header.local_sequence, header.global_id);
    return std::equal(
        readable(payload).begin(), readable(payload).end(),
        readable(expected).begin(), readable(expected).end());
}

void record_payload(
    ConsumerMetrics& metrics,
    const Payload& payload,
    const std::uint32_t expected_size,
    const std::uint64_t queue_sequence = 0) {
    ++metrics.reads;
    if (!valid_payload(payload, expected_size)) {
        ++metrics.invalid_payloads;
        ++metrics.validation_errors;
        return;
    }
    metrics.payload_ids.push_back(read_header(payload).global_id);
    if (queue_sequence != 0 && !metrics.queue_sequences.record(queue_sequence)) {
        ++metrics.validation_errors;
    }
}

[[nodiscard]] std::uint64_t parse_unsigned(
    const std::string_view option,
    const std::string_view text) {
    std::uint64_t value = 0;
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || position != end) {
        throw std::invalid_argument(std::string(option) + " requires an unsigned integer");
    }
    return value;
}

[[nodiscard]] std::vector<std::size_t> parse_consumers(
    const std::string_view text) {
    if (text == "default" || text == "legacy") {
        return {orbitqueue::benchmark::default_consumer_matrix.begin(),
                orbitqueue::benchmark::default_consumer_matrix.end()};
    }
    if (text == "single") {
        return {1};
    }

    std::vector<std::size_t> values;
    std::size_t begin = 0;
    while (begin < text.size()) {
        const auto comma = text.find(',', begin);
        const auto end = comma == std::string_view::npos ? text.size() : comma;
        const auto parsed = parse_unsigned("--consumers", text.substr(begin, end - begin));
        if (parsed == 0 || parsed > 64) {
            throw std::invalid_argument("consumer counts must be between 1 and 64");
        }
        values.push_back(static_cast<std::size_t>(parsed));
        if (comma == std::string_view::npos) {
            break;
        }
        begin = comma + 1;
    }
    if (values.empty()) {
        throw std::invalid_argument("--consumers requires a list or preset");
    }
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

void print_usage(std::ostream& output) {
    output
        << "usage: orbitqueue_benchmark [options]\n"
        << "  --duration-ms <uint64>\n"
        << "  --warmup-ms <uint64>\n"
        << "  --trials <uint32>\n"
        << "  --capacity <uint32>\n"
        << "  --payload-size <uint32>\n"
        << "  --producers <uint32>\n"
        << "  --consumers <default|single|comma-separated counts>\n"
        << "  --queue <all|spsc|blocking|spmc|mpmc|boost>\n"
        << "  --help\n";
}

[[nodiscard]] Config parse_config(const int argc, char** argv) {
    Config config;
    for (int index = 1; index < argc; ++index) {
        const std::string_view option(argv[index]);
        if (option == "--help") {
            print_usage(std::cout);
            std::exit(EXIT_SUCCESS);
        }
        if (index + 1 >= argc) {
            throw std::invalid_argument(std::string(option) + " requires a value");
        }
        const std::string_view value(argv[++index]);
        if (option == "--duration-ms") {
            config.duration_ms = parse_unsigned(option, value);
        } else if (option == "--warmup-ms") {
            config.warmup_ms = parse_unsigned(option, value);
        } else if (option == "--trials") {
            const auto parsed = parse_unsigned(option, value);
            if (parsed > std::numeric_limits<std::uint32_t>::max()) {
                throw std::invalid_argument("--trials exceeds uint32 range");
            }
            config.trials = static_cast<std::uint32_t>(parsed);
        } else if (option == "--capacity") {
            const auto parsed = parse_unsigned(option, value);
            if (parsed > std::numeric_limits<std::uint32_t>::max()) {
                throw std::invalid_argument("--capacity exceeds uint32 range");
            }
            config.capacity = static_cast<std::uint32_t>(parsed);
        } else if (option == "--payload-size") {
            const auto parsed = parse_unsigned(option, value);
            if (parsed > std::numeric_limits<std::uint32_t>::max()) {
                throw std::invalid_argument("--payload-size exceeds uint32 range");
            }
            config.payload_size = static_cast<std::uint32_t>(parsed);
        } else if (option == "--producers") {
            const auto parsed = parse_unsigned(option, value);
            if (parsed > std::numeric_limits<std::uint32_t>::max()) {
                throw std::invalid_argument("--producers exceeds uint32 range");
            }
            config.producers = static_cast<std::uint32_t>(parsed);
            config.custom_producers = true;
        } else if (option == "--consumers") {
            config.consumers = parse_consumers(value);
            config.custom_consumers = true;
        } else if (option == "--queue") {
            config.queue = value;
        } else {
            throw std::invalid_argument("unknown option: " + std::string(option));
        }
    }

    const std::array<std::string_view, 6> queues{
        "all", "spsc", "blocking", "spmc", "mpmc", "boost"};
    if (std::find(queues.begin(), queues.end(), config.queue) == queues.end()) {
        throw std::invalid_argument("invalid --queue value");
    }
    if (config.duration_ms == 0 ||
        config.duration_ms > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument("--duration-ms must fit a positive int64");
    }
    if (config.warmup_ms > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument("--warmup-ms must fit int64");
    }
    if (config.trials == 0 || config.trials > 1000) {
        throw std::invalid_argument("--trials must be between 1 and 1000");
    }
    if (config.capacity == 0 || config.capacity > 1'000'000) {
        throw std::invalid_argument("--capacity must be between 1 and 1000000");
    }
    if ((config.queue == "all" || config.queue == "mpmc") &&
        (config.capacity < 2 ||
         (config.capacity & (config.capacity - 1)) != 0)) {
        throw std::invalid_argument(
            "MPMC benchmark capacity must be a power of two greater than one");
    }
    if (config.payload_size < payload_header_size ||
        config.payload_size > max_payload_size) {
        throw std::invalid_argument("--payload-size must be between 32 and 256");
    }
    if (config.producers == 0 || config.producers > 64) {
        throw std::invalid_argument("--producers must be between 1 and 64");
    }
    return config;
}

[[nodiscard]] std::vector<Scenario> scenarios_for(const Config& config) {
    std::vector<Scenario> scenarios;
    const auto selected = [&](const std::string_view name) {
        return config.queue == "all" || config.queue == name;
    };
    if (selected("spsc")) {
        scenarios.push_back({ScenarioKind::spsc, "spsc", 1, 1,
            "work-sharing; exactly one producer and one consumer"});
    }
    if (selected("spmc")) {
        for (const auto consumers : config.consumers) {
            scenarios.push_back({ScenarioKind::spmc, "spmc_multicast", 1, consumers,
                "multicast aggregate reads are not work-sharing pops"});
        }
    }
    if (selected("blocking")) {
        for (const auto consumers : config.consumers) {
            scenarios.push_back({ScenarioKind::blocking, "blocking_mpmc",
                config.producers, consumers,
                "blocking mutex baseline; exclusive work sharing"});
        }
    }
    if (selected("mpmc")) {
        if (!config.custom_producers &&
            !config.custom_consumers) {
            constexpr std::array<std::pair<std::size_t, std::size_t>, 3> matrix{
                std::pair<std::size_t, std::size_t>{1, 1},
                std::pair<std::size_t, std::size_t>{4, 4},
                std::pair<std::size_t, std::size_t>{4, 10}};
            for (const auto [producers, consumers] : matrix) {
                scenarios.push_back({ScenarioKind::mpmc, "mpmc",
                    producers, consumers,
                    "mutex-free bounded sequence-cell MPMC; exclusive work sharing"});
            }
        } else {
            for (const auto consumers : config.consumers) {
                scenarios.push_back({ScenarioKind::mpmc, "mpmc",
                    config.producers, consumers,
                    "mutex-free bounded sequence-cell MPMC; exclusive work sharing"});
            }
        }
    }
    if (selected("boost")) {
#if defined(ORBITQUEUE_HAVE_BOOST_LOCKFREE)
        for (const auto consumers : config.consumers) {
            scenarios.push_back({ScenarioKind::boost,
                "boost_lockfree_work_sharing", 1, consumers,
                "optional Boost work-sharing baseline; lock-free status is platform-dependent"});
        }
#else
        if (config.queue == "boost") {
            throw std::invalid_argument(
                "Boost benchmark requested but boost/lockfree/queue.hpp was unavailable at build time");
        }
#endif
    }
    return scenarios;
}

[[nodiscard]] std::string timestamp_utc() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto* utc = std::gmtime(&time);
    if (utc == nullptr) {
        return "unknown";
    }
    std::ostringstream output;
    output << std::put_time(utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

[[nodiscard]] std::uint64_t count_unique_ids(
    const std::vector<ConsumerMetrics>& metrics,
    std::uint64_t& duplicate_count,
    std::uint64_t& minimum,
    std::uint64_t& maximum) {
    std::vector<std::uint64_t> ids;
    for (const auto& metric : metrics) {
        ids.insert(ids.end(), metric.payload_ids.begin(), metric.payload_ids.end());
    }
    if (ids.empty()) {
        duplicate_count = 0;
        minimum = 0;
        maximum = 0;
        return 0;
    }
    std::sort(ids.begin(), ids.end());
    minimum = ids.front();
    maximum = ids.back();
    const auto unique_end = std::unique(ids.begin(), ids.end());
    const auto unique = static_cast<std::uint64_t>(
        std::distance(ids.begin(), unique_end));
    duplicate_count = static_cast<std::uint64_t>(ids.size()) - unique;
    return unique;
}

[[nodiscard]] Result make_result(
    const Scenario& scenario,
    const Config& config,
    const Metadata& metadata,
    const std::uint32_t trial,
    const std::uint64_t published,
    const std::vector<ConsumerMetrics>& metrics,
    const std::uint64_t full_retries,
    const std::uint64_t producer_errors,
    const bool require_complete_work_sharing) {
    Result result;
    result.queue = scenario.name;
    result.trial = trial;
    result.capacity = config.capacity;
    result.payload_size = config.payload_size;
    result.producer_count = scenario.producers;
    result.consumer_count = scenario.consumers;
    result.duration_ms = config.duration_ms;
    result.warmup_ms = config.warmup_ms;
    result.messages_published = published;
    result.full_retries = full_retries;
    result.build_type = metadata.build_type;
    result.compiler = metadata.compiler;
    result.git_commit = metadata.git_commit;
    result.timestamp = metadata.timestamp;
    result.notes = scenario.notes;

    for (const auto& metric : metrics) {
        result.aggregate_reads += metric.reads;
        result.dropped_or_lagged += metric.lag_events;
        result.empty_retries += metric.empty_retries;
        result.invalid_payloads += metric.invalid_payloads;
        result.validation_errors += metric.validation_errors;
    }
    result.validation_errors += producer_errors;

    std::uint64_t duplicates = 0;
    std::uint64_t minimum = 0;
    std::uint64_t maximum = 0;
    result.unique_sequences_verified = count_unique_ids(
        metrics, duplicates, minimum, maximum);
    if (require_complete_work_sharing) {
        result.validation_errors += duplicates;
        if (result.aggregate_reads != published ||
            result.unique_sequences_verified != published ||
            (published != 0 && (minimum != 1 || maximum != published))) {
            ++result.validation_errors;
        }
    }

    const auto seconds = static_cast<double>(config.duration_ms) / 1000.0;
    result.throughput_messages_per_second =
        static_cast<double>(published) / seconds;
    result.throughput_reads_per_second =
        static_cast<double>(result.aggregate_reads) / seconds;
    return result;
}

[[nodiscard]] Result run_spsc(
    const Scenario& scenario,
    const Config& config,
    const Metadata& metadata,
    const std::uint32_t trial,
    const std::chrono::milliseconds duration) {
    orbitqueue::SPSCQueue<max_payload_size> queue(config.capacity);
    std::atomic<bool> running{true};
    std::uint64_t published = 0;
    std::uint64_t full_retries = 0;
    std::uint64_t producer_errors = 0;
    ConsumerMetrics metrics;

    std::thread producer([&] {
        std::uint64_t id = 1;
        while (running.load(std::memory_order_relaxed)) {
            const auto payload = make_payload(config.payload_size, 0, id, id);
            const auto result = queue.try_push(readable(payload));
            if (result.status == QueueStatus::success) {
                if (result.sequence != id) {
                    ++producer_errors;
                }
                published = id++;
            } else if (result.status == QueueStatus::full) {
                ++full_retries;
                std::this_thread::yield();
            } else {
                ++producer_errors;
                break;
            }
        }
    });
    std::thread consumer([&] {
        while (running.load(std::memory_order_relaxed) || !queue.empty()) {
            Payload payload;
            payload.size = config.payload_size;
            const auto result = queue.try_pop(writable(payload));
            if (result.status == QueueStatus::success) {
                record_payload(metrics, payload, config.payload_size, result.sequence);
            } else if (result.status == QueueStatus::empty) {
                ++metrics.empty_retries;
                std::this_thread::yield();
            } else {
                ++metrics.validation_errors;
                break;
            }
        }
    });

    std::this_thread::sleep_for(duration);
    running.store(false, std::memory_order_relaxed);
    producer.join();
    consumer.join();
    return make_result(scenario, config, metadata, trial, published,
                       {std::move(metrics)}, full_retries, producer_errors, true);
}

[[nodiscard]] Result run_spmc(
    const Scenario& scenario,
    const Config& config,
    const Metadata& metadata,
    const std::uint32_t trial,
    const std::chrono::milliseconds duration) {
    orbitqueue::SPMCMulticastQueue<max_payload_size> queue(config.capacity);
    std::atomic<bool> running{true};
    std::uint64_t producer_errors = 0;
    std::vector<ConsumerMetrics> metrics(scenario.consumers);
    std::vector<std::thread> consumers;
    for (std::size_t index = 0; index < scenario.consumers; ++index) {
        auto consumer = queue.register_consumer();
        consumers.emplace_back([&, index, consumer = std::move(consumer)]() mutable {
            while (running.load(std::memory_order_relaxed) ||
                   consumer.next_sequence() <= queue.published_sequence()) {
                Payload payload;
                payload.size = config.payload_size;
                const auto result = consumer.try_read(writable(payload));
                if (result.status == QueueStatus::success) {
                    record_payload(metrics[index], payload, config.payload_size,
                                   result.sequence);
                } else if (result.status == QueueStatus::consumer_lagged ||
                           result.status == QueueStatus::overwritten) {
                    ++metrics[index].lag_events;
                } else if (result.status == QueueStatus::empty) {
                    ++metrics[index].empty_retries;
                    std::this_thread::yield();
                } else {
                    ++metrics[index].validation_errors;
                    break;
                }
            }
        });
    }
    std::thread producer([&] {
        std::uint64_t id = 1;
        while (running.load(std::memory_order_relaxed)) {
            const auto payload = make_payload(config.payload_size, 0, id, id);
            const auto result = queue.try_publish(readable(payload));
            if (result.status == QueueStatus::success) {
                if (result.sequence != id) {
                    ++producer_errors;
                }
                ++id;
            } else {
                ++producer_errors;
            }
        }
    });

    std::this_thread::sleep_for(duration);
    running.store(false, std::memory_order_relaxed);
    producer.join();
    for (auto& consumer : consumers) {
        consumer.join();
    }
    return make_result(scenario, config, metadata, trial,
                       queue.published_sequence(), metrics,
                       0, producer_errors, false);
}

template <typename Push, typename Pop, typename Close>
[[nodiscard]] Result run_work_sharing(
    Push push,
    Pop pop,
    Close close,
    const Scenario& scenario,
    const Config& config,
    const Metadata& metadata,
    const std::uint32_t trial,
    const std::chrono::milliseconds duration,
    const bool has_queue_sequences) {
    std::atomic<bool> running{true};
    std::atomic<std::uint64_t> next_id{1};
    std::atomic<std::uint64_t> published{0};
    std::atomic<std::uint64_t> full_retries{0};
    std::atomic<std::uint64_t> producer_errors{0};
    std::atomic<std::size_t> remaining{scenario.producers};
    std::vector<ConsumerMetrics> metrics(scenario.consumers);

    std::vector<std::thread> consumers;
    for (std::size_t index = 0; index < scenario.consumers; ++index) {
        consumers.emplace_back([&, index] {
            while (true) {
                Payload payload;
                payload.size = config.payload_size;
                const auto result = pop(payload);
                if (result.status == QueueStatus::success) {
                    record_payload(metrics[index], payload, config.payload_size,
                                   has_queue_sequences ? result.sequence : 0);
                } else if (result.status == QueueStatus::empty) {
                    ++metrics[index].empty_retries;
                    std::this_thread::yield();
                } else if (result.status == QueueStatus::closed) {
                    break;
                } else {
                    ++metrics[index].validation_errors;
                    break;
                }
            }
        });
    }

    std::vector<std::thread> producers;
    for (std::size_t producer = 0; producer < scenario.producers; ++producer) {
        producers.emplace_back([&, producer] {
            std::uint64_t local = 1;
            while (running.load(std::memory_order_relaxed)) {
                const auto id = next_id.fetch_add(1, std::memory_order_relaxed);
                const auto payload = make_payload(
                    config.payload_size, static_cast<std::uint32_t>(producer),
                    local++, id);
                while (true) {
                    const auto result = push(payload);
                    if (result.status == QueueStatus::success) {
                        published.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                    if (result.status != QueueStatus::full) {
                        producer_errors.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                    full_retries.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::yield();
                }
            }
            if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1U) {
                close();
            }
        });
    }

    std::this_thread::sleep_for(duration);
    running.store(false, std::memory_order_relaxed);
    for (auto& producer : producers) {
        producer.join();
    }
    for (auto& consumer : consumers) {
        consumer.join();
    }
    return make_result(
        scenario, config, metadata, trial,
        published.load(std::memory_order_relaxed), metrics,
        full_retries.load(std::memory_order_relaxed),
        producer_errors.load(std::memory_order_relaxed), true);
}

[[nodiscard]] Result run_blocking(
    const Scenario& scenario,
    const Config& config,
    const Metadata& metadata,
    const std::uint32_t trial,
    const std::chrono::milliseconds duration) {
    orbitqueue::BlockingQueue<Payload> queue(config.capacity);
    const auto push = [&](const Payload& payload) {
        return orbitqueue::WriteResult{queue.try_push(payload), 0};
    };
    const auto pop = [&](Payload& payload) {
        const auto value = queue.pop();
        if (!value) {
            return orbitqueue::ReadResult{QueueStatus::closed, 0, 0};
        }
        payload = *value;
        return orbitqueue::ReadResult{QueueStatus::success, payload.size, 0};
    };
    return run_work_sharing(
        push, pop, [&] { queue.close(); }, scenario, config,
        metadata, trial, duration, false);
}

[[nodiscard]] Result run_mpmc(
    const Scenario& scenario,
    const Config& config,
    const Metadata& metadata,
    const std::uint32_t trial,
    const std::chrono::milliseconds duration) {
    orbitqueue::MPMCQueue<max_payload_size> queue(config.capacity);
    std::atomic<bool> producers_done{false};
    const auto push = [&](const Payload& payload) {
        return queue.try_push(readable(payload));
    };
    const auto pop = [&](Payload& payload) {
        const auto result = queue.try_pop(writable(payload));
        if (result.status == QueueStatus::empty &&
            producers_done.load(std::memory_order_acquire)) {
            return orbitqueue::ReadResult{QueueStatus::closed, 0, 0};
        }
        return result;
    };
    return run_work_sharing(
        push, pop,
        [&] { producers_done.store(true, std::memory_order_release); },
        scenario, config,
        metadata, trial, duration, true);
}

#if defined(ORBITQUEUE_HAVE_BOOST_LOCKFREE)
[[nodiscard]] Result run_boost(
    const Scenario& scenario,
    const Config& config,
    const Metadata& metadata,
    const std::uint32_t trial,
    const std::chrono::milliseconds duration) {
    boost::lockfree::queue<Payload, boost::lockfree::fixed_sized<true>> queue(
        config.capacity);
    std::atomic<bool> producer_done{false};
    const auto push = [&](const Payload& payload) {
        return orbitqueue::WriteResult{
            queue.push(payload) ? QueueStatus::success : QueueStatus::full, 0};
    };
    const auto pop = [&](Payload& payload) {
        if (queue.pop(payload)) {
            return orbitqueue::ReadResult{QueueStatus::success, payload.size, 0};
        }
        return orbitqueue::ReadResult{
            producer_done.load(std::memory_order_acquire)
                ? QueueStatus::closed
                : QueueStatus::empty,
            0, 0};
    };
    const auto close = [&] { producer_done.store(true, std::memory_order_release); };
    return run_work_sharing(
        push, pop, close, scenario, config,
        metadata, trial, duration, false);
}
#endif

[[nodiscard]] Result run_scenario(
    const Scenario& scenario,
    const Config& config,
    const Metadata& metadata,
    const std::uint32_t trial,
    const std::chrono::milliseconds duration) {
    switch (scenario.kind) {
    case ScenarioKind::spsc:
        return run_spsc(scenario, config, metadata, trial, duration);
    case ScenarioKind::blocking:
        return run_blocking(scenario, config, metadata, trial, duration);
    case ScenarioKind::spmc:
        return run_spmc(scenario, config, metadata, trial, duration);
    case ScenarioKind::mpmc:
        return run_mpmc(scenario, config, metadata, trial, duration);
    case ScenarioKind::boost:
#if defined(ORBITQUEUE_HAVE_BOOST_LOCKFREE)
        return run_boost(scenario, config, metadata, trial, duration);
#else
        throw std::logic_error("Boost scenario was not compiled");
#endif
    }
    throw std::logic_error("unknown benchmark scenario");
}

} // namespace

int main(int argc, char** argv) {
    Config config;
    try {
        config = parse_config(argc, argv);
        const auto scenarios = scenarios_for(config);
        const Metadata metadata{
            ORBITQUEUE_BENCHMARK_BUILD_TYPE,
            ORBITQUEUE_BENCHMARK_COMPILER,
            ORBITQUEUE_BENCHMARK_GIT_COMMIT,
            timestamp_utc()};

        bool valid = true;
        for (const auto& scenario : scenarios) {
            if (config.warmup_ms != 0) {
                auto warmup_config = config;
                warmup_config.duration_ms = config.warmup_ms;
                const auto warmup = run_scenario(
                    scenario, warmup_config, metadata, 0,
                    std::chrono::milliseconds(config.warmup_ms));
                if (warmup.validation_errors != 0) {
                    std::cerr << "benchmark warmup validation failed: "
                              << orbitqueue::benchmark::to_json(warmup) << '\n';
                    valid = false;
                    continue;
                }
            }
            for (std::uint32_t trial = 1; trial <= config.trials; ++trial) {
                const auto result = run_scenario(
                    scenario, config, metadata, trial,
                    std::chrono::milliseconds(config.duration_ms));
                std::cout << orbitqueue::benchmark::to_json(result) << '\n';
                valid = result.validation_errors == 0 && valid;
            }
        }
        return valid ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << "orbitqueue_benchmark: " << error.what() << '\n';
        print_usage(std::cerr);
        return EXIT_FAILURE;
    }
}
