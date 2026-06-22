#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

#include "orbitqueue/result.h"

namespace orbitqueue {

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(const std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("BlockingQueue capacity must be greater than zero");
        }
    }

    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    [[nodiscard]] QueueStatus push(const T& item) {
        std::unique_lock lock(mutex_);
        not_full_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });
        if (closed_) {
            return QueueStatus::closed;
        }
        queue_.push(item);
        lock.unlock();
        not_empty_.notify_one();
        return QueueStatus::success;
    }

    [[nodiscard]] QueueStatus try_push(const T& item) {
        std::lock_guard lock(mutex_);
        if (closed_) {
            return QueueStatus::closed;
        }
        if (queue_.size() == capacity_) {
            return QueueStatus::full;
        }
        queue_.push(item);
        not_empty_.notify_one();
        return QueueStatus::success;
    }

    [[nodiscard]] std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        lock.unlock();
        not_full_.notify_one();
        return item;
    }

    [[nodiscard]] QueueStatus try_pop(T& destination) {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) {
            return closed_ ? QueueStatus::closed : QueueStatus::empty;
        }
        destination = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return QueueStatus::success;
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    [[nodiscard]] bool closed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T> queue_;
    bool closed_{};
};

} // namespace orbitqueue
