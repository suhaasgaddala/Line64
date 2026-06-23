#pragma once

#include "benchmark_message.h"
#include "orbitqueue/result.h"

#include <boost/lockfree/queue.hpp>

namespace orbitqueue::benchmark {

class BoostLockfreeMPMCAdapter {
public:
    static constexpr bool has_queue_sequences = false;

    explicit BoostLockfreeMPMCAdapter(const std::size_t capacity)
        : queue_(capacity) {}

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "boost::lockfree::queue";
    }

    [[nodiscard]] static constexpr const char* delivery_semantics() noexcept {
        return "exclusive_pop";
    }

    [[nodiscard]] orbitqueue::WriteResult try_push(const Payload& payload) {
        return orbitqueue::WriteResult{
            queue_.push(payload)
                ? orbitqueue::QueueStatus::success
                : orbitqueue::QueueStatus::full,
            0};
    }

    [[nodiscard]] orbitqueue::ReadResult try_pop(Payload& payload) {
        if (queue_.pop(payload)) {
            return orbitqueue::ReadResult{
                orbitqueue::QueueStatus::success, payload.size, 0};
        }
        return orbitqueue::ReadResult{orbitqueue::QueueStatus::empty, 0, 0};
    }

private:
    boost::lockfree::queue<Payload, boost::lockfree::fixed_sized<true>> queue_;
};

} // namespace orbitqueue::benchmark
