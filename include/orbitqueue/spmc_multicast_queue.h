#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "orbitqueue/fixed_message.h"

namespace orbitqueue {

template <std::size_t MaxPayloadSize>
class SPMCMulticastQueue {
public:
    class Consumer {
    public:
        Consumer(const Consumer&) = delete;
        Consumer& operator=(const Consumer&) = delete;

        Consumer(Consumer&& other) noexcept
            : queue_(std::exchange(other.queue_, nullptr)),
              next_sequence_(other.next_sequence_) {}

        Consumer& operator=(Consumer&& other) noexcept {
            if (this != &other) {
                queue_ = std::exchange(other.queue_, nullptr);
                next_sequence_ = other.next_sequence_;
            }
            return *this;
        }

        [[nodiscard]] ReadResult try_read(
            const std::span<std::byte> destination) {
            if (queue_ == nullptr) {
                return {QueueStatus::invalid_consumer, 0, next_sequence_};
            }
            return queue_->try_read(*this, destination);
        }

        [[nodiscard]] std::uint64_t next_sequence() const noexcept {
            return next_sequence_;
        }

    private:
        friend class SPMCMulticastQueue;
        Consumer(SPMCMulticastQueue& queue, const std::uint64_t next_sequence) noexcept
            : queue_(&queue), next_sequence_(next_sequence) {}

        SPMCMulticastQueue* queue_;
        std::uint64_t next_sequence_;
    };

    explicit SPMCMulticastQueue(const std::size_t capacity)
        : capacity_(capacity), slots_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument(
                "SPMCMulticastQueue capacity must be greater than zero");
        }
    }

    SPMCMulticastQueue(const SPMCMulticastQueue&) = delete;
    SPMCMulticastQueue& operator=(const SPMCMulticastQueue&) = delete;

    [[nodiscard]] Consumer register_consumer() {
        std::lock_guard lock(mutex_);
        return Consumer(*this, published_sequence_ + 1);
    }

    [[nodiscard]] WriteResult try_publish(
        const std::span<const std::byte> payload) {
        if (payload.size() > MaxPayloadSize) {
            return {QueueStatus::message_too_large, 0};
        }
        std::lock_guard lock(mutex_);
        const auto sequence = published_sequence_ + 1;
        const auto result = slots_[(sequence - 1) % capacity_].assign(payload, sequence);
        published_sequence_ = sequence;
        return result;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] std::uint64_t published_sequence() const {
        std::lock_guard lock(mutex_);
        return published_sequence_;
    }

private:
    [[nodiscard]] ReadResult try_read(
        Consumer& consumer, const std::span<std::byte> destination) {
        std::lock_guard lock(mutex_);
        if (consumer.next_sequence_ > published_sequence_) {
            return {QueueStatus::empty, 0, consumer.next_sequence_};
        }

        const auto oldest = published_sequence_ >= capacity_
            ? published_sequence_ - capacity_ + 1
            : 1;
        if (consumer.next_sequence_ < oldest) {
            consumer.next_sequence_ = oldest;
            return {QueueStatus::consumer_lagged, 0, oldest};
        }

        const auto expected = consumer.next_sequence_;
        const auto& slot = slots_[(expected - 1) % capacity_];
        if (slot.sequence() != expected) {
            consumer.next_sequence_ = oldest;
            return {QueueStatus::overwritten, 0, oldest};
        }

        const auto result = slot.copy_to(destination);
        if (ok(result.status)) {
            ++consumer.next_sequence_;
        }
        return result;
    }

    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::vector<FixedMessage<MaxPayloadSize>> slots_;
    std::uint64_t published_sequence_{};
};

} // namespace orbitqueue
