#pragma once

#include "benchmark_message.h"
#include "orbitqueue/blocking_queue.h"

namespace orbitqueue::benchmark {

class Line64BlockingQueueAdapter {
public:
    static constexpr bool has_queue_sequences = false;

    explicit Line64BlockingQueueAdapter(const std::size_t capacity)
        : queue_(capacity) {}

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "Line64::BlockingQueue";
    }

    [[nodiscard]] static constexpr const char* delivery_semantics() noexcept {
        return "exclusive_pop";
    }

    [[nodiscard]] orbitqueue::WriteResult try_push(const Payload& payload) {
        return orbitqueue::WriteResult{queue_.try_push(payload), 0};
    }

    [[nodiscard]] orbitqueue::ReadResult try_pop(Payload& payload) {
        const auto value = queue_.pop();
        if (!value) {
            return orbitqueue::ReadResult{orbitqueue::QueueStatus::closed, 0, 0};
        }
        payload = *value;
        return orbitqueue::ReadResult{
            orbitqueue::QueueStatus::success, payload.size, 0};
    }

    void close() {
        queue_.close();
    }

private:
    orbitqueue::BlockingQueue<Payload> queue_;
};

} // namespace orbitqueue::benchmark
