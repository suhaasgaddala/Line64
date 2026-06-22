#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "orbitqueue/result.h"

namespace orbitqueue {

template <std::size_t MaxPayloadSize>
class FixedMessage {
public:
    [[nodiscard]] WriteResult assign(
        const std::span<const std::byte> payload,
        const std::uint64_t sequence) noexcept {
        if (payload.size() > MaxPayloadSize) {
            return {QueueStatus::message_too_large, sequence};
        }
        std::copy(payload.begin(), payload.end(), storage_.begin());
        size_ = payload.size();
        sequence_ = sequence;
        return {QueueStatus::success, sequence_};
    }

    [[nodiscard]] ReadResult copy_to(
        const std::span<std::byte> destination) const noexcept {
        if (destination.size() < size_) {
            return {QueueStatus::message_too_large, 0, sequence_};
        }
        std::copy_n(storage_.begin(), size_, destination.begin());
        return {QueueStatus::success, size_, sequence_};
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr std::uint64_t sequence() const noexcept { return sequence_; }
    [[nodiscard]] static constexpr std::size_t max_payload_size() noexcept {
        return MaxPayloadSize;
    }

private:
    std::array<std::byte, MaxPayloadSize> storage_{};
    std::size_t size_{};
    std::uint64_t sequence_{};
};

} // namespace orbitqueue
