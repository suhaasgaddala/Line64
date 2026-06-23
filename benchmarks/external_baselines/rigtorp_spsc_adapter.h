#pragma once

#include "benchmark_message.h"
#include "orbitqueue/result.h"

#include <rigtorp/SPSCQueue.h>

namespace orbitqueue::benchmark {

class RigtorpSPSCAdapter {
public:
    static constexpr bool has_queue_sequences = false;

    explicit RigtorpSPSCAdapter(const std::size_t capacity)
        : queue_(capacity) {}

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "rigtorp::SPSCQueue";
    }

    [[nodiscard]] static constexpr const char* delivery_semantics() noexcept {
        return "exclusive_handoff";
    }

    [[nodiscard]] orbitqueue::WriteResult try_push(const Payload& payload) {
        return orbitqueue::WriteResult{
            queue_.try_push(payload)
                ? orbitqueue::QueueStatus::success
                : orbitqueue::QueueStatus::full,
            0};
    }

    [[nodiscard]] orbitqueue::ReadResult try_pop(Payload& payload) {
        auto* value = queue_.front();
        if (value == nullptr) {
            return orbitqueue::ReadResult{orbitqueue::QueueStatus::empty, 0, 0};
        }
        payload = *value;
        queue_.pop();
        return orbitqueue::ReadResult{
            orbitqueue::QueueStatus::success, payload.size, 0};
    }

private:
    rigtorp::SPSCQueue<Payload> queue_;
};

} // namespace orbitqueue::benchmark
