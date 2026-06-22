#include <array>
#include <cstddef>

#include "orbitqueue/fixed_message.h"
#include "test_support.h"

using orbitqueue::FixedMessage;
using orbitqueue::QueueStatus;
using test_support::expect;

void run_fixed_message_tests() {
    FixedMessage<4> message;
    std::array<std::byte, 4> destination{};

    expect(message.assign({}, 1).status == QueueStatus::success,
           "zero-byte message must be accepted");
    expect(message.copy_to(destination).bytes_read == 0,
           "zero-byte message must copy zero bytes");

    const std::array one{std::byte{0x2a}};
    expect(message.assign(one, 2).status == QueueStatus::success,
           "one-byte message must be accepted");
    const auto one_read = message.copy_to(destination);
    expect(one_read.bytes_read == 1 && destination[0] == one[0],
           "one-byte message must round trip");

    const std::array exact{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    expect(message.assign(exact, 3).status == QueueStatus::success,
           "maximum-size message must be accepted");
    expect(message.copy_to(destination).sequence == 3 && destination == exact,
           "maximum-size message must round trip with its sequence");

    const std::array oversized{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}};
    expect(message.assign(oversized, 4).status == QueueStatus::message_too_large,
           "oversized message must be rejected");
    expect(message.sequence() == 3,
           "rejected writes must preserve the prior message");

    std::array<std::byte, 3> short_destination{};
    expect(message.copy_to(short_destination).status == QueueStatus::message_too_large,
           "short destination must be rejected");
}
