#pragma once

#include "benchmark_message.h"
#include "orbitqueue/result.h"

#include <concurrentqueue.h>

namespace orbitqueue::benchmark {

class MoodycamelMPMCAdapter {
public:
    static constexpr bool has_queue_sequences = false;

    explicit MoodycamelMPMCAdapter(const std::size_t capacity)
        : queue_(capacity) {}

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "moodycamel::ConcurrentQueue";
    }

    [[nodiscard]] static constexpr const char* delivery_semantics() noexcept {
        return "exclusive_pop";
    }

    [[nodiscard]] orbitqueue::WriteResult try_push(const Payload& payload) {
        return orbitqueue::WriteResult{
            queue_.try_enqueue(payload)
                ? orbitqueue::QueueStatus::success
                : orbitqueue::QueueStatus::full,
            0};
    }

    [[nodiscard]] orbitqueue::ReadResult try_pop(Payload& payload) {
        if (queue_.try_dequeue(payload)) {
            return orbitqueue::ReadResult{
                orbitqueue::QueueStatus::success, payload.size, 0};
        }
        return orbitqueue::ReadResult{orbitqueue::QueueStatus::empty, 0, 0};
    }

private:
    moodycamel::ConcurrentQueue<Payload> queue_;
};

} // namespace orbitqueue::benchmark
