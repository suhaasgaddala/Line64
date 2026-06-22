#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include "orbitqueue/fixed_message.h"

namespace orbitqueue {

template <std::size_t MaxPayloadSize>
class SPSCQueue {
public:
    explicit SPSCQueue(const std::size_t capacity)
        : capacity_(capacity), slots_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("SPSCQueue capacity must be greater than zero");
        }
    }

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    [[nodiscard]] WriteResult try_push(const std::span<const std::byte> payload) noexcept {
        if (payload.size() > MaxPayloadSize) {
            return {QueueStatus::message_too_large, 0};
        }

        const auto head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) == capacity_) {
            return {QueueStatus::full, 0};
        }

        const auto sequence = head + 1;
        const auto result = slots_[head % capacity_].assign(payload, sequence);
        head_.store(sequence, std::memory_order_release);
        return result;
    }

    [[nodiscard]] ReadResult try_pop(const std::span<std::byte> destination) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return {QueueStatus::empty, 0, 0};
        }

        const auto result = slots_[tail % capacity_].copy_to(destination);
        if (ok(result.status)) {
            tail_.store(tail + 1, std::memory_order_release);
        }
        return result;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool full() const noexcept {
        return head_.load(std::memory_order_acquire) -
                   tail_.load(std::memory_order_acquire) ==
               capacity_;
    }

private:
    const std::size_t capacity_;
    std::vector<FixedMessage<MaxPayloadSize>> slots_;
    alignas(64) std::atomic<std::uint64_t> head_{};
    alignas(64) std::atomic<std::uint64_t> tail_{};
};

} // namespace orbitqueue
