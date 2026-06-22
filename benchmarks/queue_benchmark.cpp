#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "orbitqueue/spmc_multicast_queue.h"
#include "orbitqueue/spsc_queue.h"

namespace {

using Clock = std::chrono::steady_clock;
using orbitqueue::QueueStatus;

template <typename T>
std::span<const std::byte> bytes_of(const T& value) noexcept {
    return std::as_bytes(std::span{&value, std::size_t{1}});
}

template <typename T>
std::span<std::byte> writable_bytes_of(T& value) noexcept {
    return std::as_writable_bytes(std::span{&value, std::size_t{1}});
}

void print_result(
    const char* queue_name,
    const std::size_t capacity,
    const std::size_t payload_size,
    const std::size_t producer_count,
    const std::size_t consumer_count,
    const std::uint64_t duration_ms,
    const std::uint64_t published,
    const std::uint64_t aggregate_reads,
    const std::uint64_t unique_sequences,
    const std::uint64_t lagged) {
    std::cout << "{\"queue\":\"" << queue_name
              << "\",\"capacity\":" << capacity
              << ",\"payload_size\":" << payload_size
              << ",\"producer_count\":" << producer_count
              << ",\"consumer_count\":" << consumer_count
              << ",\"duration_ms\":" << duration_ms
              << ",\"messages_published\":" << published
              << ",\"aggregate_reads\":" << aggregate_reads
              << ",\"unique_sequences_verified\":" << unique_sequences
              << ",\"dropped_or_lagged\":" << lagged << "}\n";
}

void benchmark_spsc(const std::chrono::milliseconds duration) {
    constexpr std::size_t capacity = 1024;
    orbitqueue::SPSCQueue<sizeof(std::uint64_t)> queue(capacity);
    std::atomic<bool> running{true};
    std::uint64_t published = 0;
    std::set<std::uint64_t> verified;

    std::thread producer([&] {
        std::uint64_t value = 1;
        while (running.load(std::memory_order_relaxed)) {
            const auto result = queue.try_push(bytes_of(value));
            if (result.status == QueueStatus::success) {
                published = result.sequence;
                ++value;
            } else {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        while (running.load(std::memory_order_relaxed) || !queue.empty()) {
            std::uint64_t value = 0;
            const auto result = queue.try_pop(writable_bytes_of(value));
            if (result.status == QueueStatus::success) {
                if (value == result.sequence) {
                    verified.insert(result.sequence);
                }
            } else {
                std::this_thread::yield();
            }
        }
    });

    std::this_thread::sleep_for(duration);
    running.store(false, std::memory_order_relaxed);
    producer.join();
    consumer.join();
    print_result("spsc", capacity, sizeof(std::uint64_t), 1, 1,
                 static_cast<std::uint64_t>(duration.count()), published,
                 verified.size(), verified.size(), 0);
}

struct ConsumerMetrics {
    std::uint64_t reads{};
    std::uint64_t lagged{};
    std::set<std::uint64_t> verified;
};

void benchmark_spmc(const std::chrono::milliseconds duration) {
    constexpr std::size_t capacity = 1024;
    constexpr std::size_t consumer_count = 2;
    orbitqueue::SPMCMulticastQueue<sizeof(std::uint64_t)> queue(capacity);
    std::atomic<bool> running{true};
    std::vector<ConsumerMetrics> metrics(consumer_count);
    std::vector<std::thread> consumers;
    consumers.reserve(consumer_count);

    for (std::size_t index = 0; index < consumer_count; ++index) {
        auto consumer = queue.register_consumer();
        consumers.emplace_back(
            [&, index, consumer = std::move(consumer)]() mutable {
                while (running.load(std::memory_order_relaxed) ||
                       consumer.next_sequence() <= queue.published_sequence()) {
                    std::uint64_t value = 0;
                    const auto result = consumer.try_read(writable_bytes_of(value));
                    if (result.status == QueueStatus::success) {
                        ++metrics[index].reads;
                        if (value == result.sequence) {
                            metrics[index].verified.insert(result.sequence);
                        }
                    } else if (result.status == QueueStatus::consumer_lagged ||
                               result.status == QueueStatus::overwritten) {
                        ++metrics[index].lagged;
                    } else {
                        std::this_thread::yield();
                    }
                }
            });
    }

    std::thread producer([&] {
        std::uint64_t value = 1;
        while (running.load(std::memory_order_relaxed)) {
            const auto result = queue.try_publish(bytes_of(value));
            if (result.status == QueueStatus::success) {
                ++value;
            }
        }
    });

    std::this_thread::sleep_for(duration);
    running.store(false, std::memory_order_relaxed);
    producer.join();
    for (auto& consumer : consumers) {
        consumer.join();
    }

    std::uint64_t aggregate_reads = 0;
    std::uint64_t lagged = 0;
    std::set<std::uint64_t> unique_verified;
    for (const auto& metric : metrics) {
        aggregate_reads += metric.reads;
        lagged += metric.lagged;
        unique_verified.insert(metric.verified.begin(), metric.verified.end());
    }
    print_result("spmc_multicast", capacity, sizeof(std::uint64_t), 1,
                 consumer_count, static_cast<std::uint64_t>(duration.count()),
                 queue.published_sequence(), aggregate_reads,
                 unique_verified.size(), lagged);
}

} // namespace

int main(int argc, char** argv) {
    std::uint64_t duration_ms = 250;
    if (argc == 2) {
        try {
            duration_ms = std::stoull(argv[1]);
        } catch (const std::exception&) {
            std::cerr << "usage: orbitqueue_benchmark [duration_ms]\n";
            return EXIT_FAILURE;
        }
    }
    if (duration_ms == 0) {
        std::cerr << "duration_ms must be greater than zero\n";
        return EXIT_FAILURE;
    }

    const auto duration = std::chrono::milliseconds(duration_ms);
    benchmark_spsc(duration);
    benchmark_spmc(duration);
    return EXIT_SUCCESS;
}
