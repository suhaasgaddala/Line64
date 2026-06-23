#pragma once

#include "benchmark_message.h"
#include "orbitqueue/result.h"

#include <boost/lockfree/spsc_queue.hpp>

namespace orbitqueue::benchmark {

class BoostLockfreeSPSCAdapter {
public:
    static constexpr bool has_queue_sequences = false;

    explicit BoostLockfreeSPSCAdapter(const std::size_t capacity)
        : queue_(capacity) {}

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "boost::lockfree::spsc_queue";
    }

    [[nodiscard]] static constexpr const char* delivery_semantics() noexcept {
        return "exclusive_handoff";
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
    boost::lockfree::spsc_queue<Payload> queue_;
};

} // namespace orbitqueue::benchmark
