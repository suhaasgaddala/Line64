#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <thread>

#include "orbitqueue/spsc_queue.h"
#include "test_support.h"

using orbitqueue::QueueStatus;
using orbitqueue::SPSCQueue;
using test_support::bytes_of;
using test_support::expect;
using test_support::writable_bytes_of;

void run_spsc_queue_tests() {
    bool rejected_zero = false;
    try {
        SPSCQueue<8> invalid(0);
    } catch (const std::invalid_argument&) {
        rejected_zero = true;
    }
    expect(rejected_zero, "SPSC queue must reject zero capacity");

    SPSCQueue<8> queue(2);
    std::uint64_t output = 0;
    expect(queue.empty(), "new SPSC queue must be empty");
    expect(queue.try_pop(writable_bytes_of(output)).status == QueueStatus::empty,
           "empty SPSC pop must report empty");

    const std::uint64_t first = 11;
    const std::uint64_t second = 22;
    expect(queue.try_push(bytes_of(first)).sequence == 1, "first sequence must be one");
    expect(queue.try_push(bytes_of(second)).sequence == 2, "second sequence must be two");
    expect(queue.full(), "SPSC queue must report full at capacity");
    expect(queue.try_push(bytes_of(first)).status == QueueStatus::full,
           "SPSC queue must not overwrite unread messages");
    expect(queue.try_pop(writable_bytes_of(output)).sequence == 1 && output == first,
           "SPSC queue must return the first message first");
    expect(queue.try_pop(writable_bytes_of(output)).sequence == 2 && output == second,
           "SPSC queue must preserve FIFO order");

    const std::array<std::byte, 9> oversized{};
    expect(queue.try_push(oversized).status == QueueStatus::message_too_large,
           "SPSC queue must reject oversized messages");

    SPSCQueue<8> retry_queue(1);
    expect(retry_queue.try_push(bytes_of(first)).status == QueueStatus::success,
           "SPSC retry setup push must succeed");
    std::array<std::byte, 4> short_destination{};
    expect(retry_queue.try_pop(short_destination).status ==
               QueueStatus::message_too_large,
           "undersized SPSC output must be rejected");
    output = 0;
    const auto retried = retry_queue.try_pop(writable_bytes_of(output));
    expect(retried.status == QueueStatus::success && retried.sequence == 1 &&
               output == first,
           "undersized SPSC output must not consume the message");

    SPSCQueue<0> zero_payload_queue(1);
    expect(zero_payload_queue.try_push(std::span<const std::byte>{}).status ==
               QueueStatus::success,
           "zero-byte SPSC payload must be accepted");
    expect(zero_payload_queue.try_pop(std::span<std::byte>{}).status ==
               QueueStatus::success,
           "zero-byte SPSC payload must round trip");

    SPSCQueue<8> stress(7);
    constexpr std::uint64_t count = 50'000;
    std::atomic<bool> sequence_error{false};
    std::thread producer([&] {
        for (std::uint64_t value = 1; value <= count; ++value) {
            while (stress.try_push(bytes_of(value)).status == QueueStatus::full) {
                std::this_thread::yield();
            }
        }
    });
    std::thread consumer([&] {
        for (std::uint64_t expected = 1; expected <= count; ++expected) {
            std::uint64_t value = 0;
            orbitqueue::ReadResult result{};
            do {
                result = stress.try_pop(writable_bytes_of(value));
                if (result.status == QueueStatus::empty) {
                    std::this_thread::yield();
                }
            } while (result.status == QueueStatus::empty);
            if (result.status != QueueStatus::success ||
                result.sequence != expected || value != expected) {
                sequence_error.store(true, std::memory_order_relaxed);
                return;
            }
        }
    });
    producer.join();
    consumer.join();
    expect(!sequence_error.load(),
           "SPSC queue must preserve payloads and sequences across wraparound");
}
