#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "orbitqueue/spmc_multicast_queue.h"
#include "test_support.h"

using orbitqueue::QueueStatus;
using orbitqueue::SPMCMulticastQueue;
using test_support::bytes_of;
using test_support::expect;
using test_support::writable_bytes_of;

void run_spmc_multicast_queue_tests() {
    bool rejected_zero = false;
    try {
        SPMCMulticastQueue<8> invalid(0);
    } catch (const std::invalid_argument&) {
        rejected_zero = true;
    }
    expect(rejected_zero, "multicast queue must reject zero capacity");

    SPMCMulticastQueue<8> queue(3);
    auto first_consumer = queue.register_consumer();
    auto second_consumer = queue.register_consumer();
    const std::uint64_t first = 101;
    const std::uint64_t second = 202;
    expect(queue.try_publish(bytes_of(first)).sequence == 1,
           "first multicast publication must use sequence one");
    expect(queue.try_publish(bytes_of(second)).sequence == 2,
           "second multicast publication must advance sequence");

    std::uint64_t output = 0;
    expect(first_consumer.try_read(writable_bytes_of(output)).sequence == 1 && output == first,
           "first consumer must see first publication");
    expect(first_consumer.try_read(writable_bytes_of(output)).sequence == 2 && output == second,
           "first consumer must advance independently");
    expect(second_consumer.try_read(writable_bytes_of(output)).sequence == 1 && output == first,
           "second consumer must also see first publication");
    expect(second_consumer.try_read(writable_bytes_of(output)).sequence == 2 && output == second,
           "second consumer must also see second publication");
    expect(second_consumer.try_read(writable_bytes_of(output)).status == QueueStatus::empty,
           "caught-up multicast consumer must report empty");

    SPMCMulticastQueue<8> wrapping(2);
    auto slow = wrapping.register_consumer();
    for (std::uint64_t value = 1; value <= 3; ++value) {
        expect(wrapping.try_publish(bytes_of(value)).status == QueueStatus::success,
               "multicast publication must succeed");
    }
    const auto lagged = slow.try_read(writable_bytes_of(output));
    expect(lagged.status == QueueStatus::consumer_lagged && slow.next_sequence() == 2,
           "slow consumer must detect lag and move to oldest retained sequence");
    expect(slow.try_read(writable_bytes_of(output)).sequence == 2 && output == 2,
           "lagged consumer must resume at oldest retained message");
    expect(slow.try_read(writable_bytes_of(output)).sequence == 3 && output == 3,
           "lagged consumer must continue through wraparound");

    auto moved_from = wrapping.register_consumer();
    auto moved_to = std::move(moved_from);
    expect(moved_from.try_read(writable_bytes_of(output)).status ==
               QueueStatus::invalid_consumer,
           "moved-from consumer must report invalid_consumer");
    expect(moved_to.next_sequence() == 4,
           "moved consumer must preserve its cursor");

    const std::array<std::byte, 9> oversized{};
    expect(wrapping.try_publish(oversized).status == QueueStatus::message_too_large,
           "multicast queue must reject oversized messages");
}
