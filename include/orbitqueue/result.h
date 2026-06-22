#pragma once

#include <cstddef>
#include <cstdint>

namespace orbitqueue {

enum class QueueStatus {
    success,
    empty,
    full,
    closed,
    message_too_large,
    invalid_capacity,
    invalid_consumer,
    consumer_lagged,
    overwritten,
    would_block
};

struct WriteResult {
    QueueStatus status{};
    std::uint64_t sequence{};
};

struct ReadResult {
    QueueStatus status{};
    std::size_t bytes_read{};
    std::uint64_t sequence{};
};

[[nodiscard]] constexpr bool ok(const QueueStatus status) noexcept {
    return status == QueueStatus::success;
}

[[nodiscard]] constexpr const char* to_string(const QueueStatus status) noexcept {
    switch (status) {
    case QueueStatus::success: return "success";
    case QueueStatus::empty: return "empty";
    case QueueStatus::full: return "full";
    case QueueStatus::closed: return "closed";
    case QueueStatus::message_too_large: return "message_too_large";
    case QueueStatus::invalid_capacity: return "invalid_capacity";
    case QueueStatus::invalid_consumer: return "invalid_consumer";
    case QueueStatus::consumer_lagged: return "consumer_lagged";
    case QueueStatus::overwritten: return "overwritten";
    case QueueStatus::would_block: return "would_block";
    }
    return "unknown";
}

} // namespace orbitqueue
