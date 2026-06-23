#pragma once

#include "benchmark_message.h"
#include "orbitqueue/result.h"

#include <atomic_queue/atomic_queue.h>

namespace orbitqueue::benchmark {

class AtomicQueueMPMCAdapter {
public:
    static constexpr bool has_queue_sequences = false;

    explicit AtomicQueueMPMCAdapter(const std::size_t capacity)
        : queue_(static_cast<unsigned>(capacity)) {}

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "atomic_queue::AtomicQueueB2";
    }

    [[nodiscard]] static constexpr const char* delivery_semantics() noexcept {
        return "exclusive_pop";
    }

    [[nodiscard]] orbitqueue::WriteResult try_push(const Payload& payload) {
        return orbitqueue::WriteResult{
            queue_.try_push(payload)
                ? orbitqueue::QueueStatus::success
                : orbitqueue::QueueStatus::full,
            0};
    }

    [[nodiscard]] orbitqueue::ReadResult try_pop(Payload& payload) {
        if (queue_.try_pop(payload)) {
            return orbitqueue::ReadResult{
                orbitqueue::QueueStatus::success, payload.size, 0};
        }
        return orbitqueue::ReadResult{orbitqueue::QueueStatus::empty, 0, 0};
    }

private:
    atomic_queue::AtomicQueueB2<Payload> queue_;
};

} // namespace orbitqueue::benchmark
