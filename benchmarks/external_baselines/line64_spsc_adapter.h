#pragma once

#include "benchmark_message.h"
#include "orbitqueue/spsc_queue.h"

namespace orbitqueue::benchmark {

class Line64SPSCAdapter {
public:
    static constexpr bool has_queue_sequences = true;

    explicit Line64SPSCAdapter(const std::size_t capacity)
        : queue_(capacity) {}

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "Line64::SPSCQueue";
    }

    [[nodiscard]] static constexpr const char* delivery_semantics() noexcept {
        return "exclusive_handoff";
    }

    [[nodiscard]] orbitqueue::WriteResult try_push(const Payload& payload) {
        return queue_.try_push(readable(payload));
    }

    [[nodiscard]] orbitqueue::ReadResult try_pop(Payload& payload) {
        return queue_.try_pop(writable(payload));
    }

private:
    orbitqueue::SPSCQueue<max_payload_size> queue_;
};

} // namespace orbitqueue::benchmark
