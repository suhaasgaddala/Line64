#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

#include "orbitqueue/blocking_queue.h"
#include "test_support.h"

using namespace std::chrono_literals;
using orbitqueue::BlockingQueue;
using orbitqueue::QueueStatus;
using test_support::expect;

void run_blocking_queue_tests() {
    bool rejected_zero = false;
    try {
        BlockingQueue<int> invalid(0);
    } catch (const std::invalid_argument&) {
        rejected_zero = true;
    }
    expect(rejected_zero, "blocking queue must reject zero capacity");

    BlockingQueue<int> queue(2);
    int empty_destination = 0;
    expect(queue.try_pop(empty_destination) == QueueStatus::empty,
           "new blocking queue must be empty");
    expect(queue.try_push(10) == QueueStatus::success, "first push must succeed");
    expect(queue.try_push(20) == QueueStatus::success, "second push must succeed");
    expect(queue.try_push(30) == QueueStatus::full, "bounded queue must report full");
    auto first = queue.pop();
    auto second = queue.pop();
    expect(first == 10 && second == 20, "blocking queue must preserve FIFO order");

    BlockingQueue<int> blocked_consumer_queue(1);
    auto blocked_consumer = std::async(std::launch::async, [&] {
        return blocked_consumer_queue.pop();
    });
    expect(blocked_consumer.wait_for(20ms) == std::future_status::timeout,
           "pop must block while the queue is open and empty");
    blocked_consumer_queue.close();
    expect(blocked_consumer.wait_for(1s) == std::future_status::ready &&
               !blocked_consumer.get().has_value(),
           "close must wake a blocked consumer");

    BlockingQueue<int> blocked_producer_queue(1);
    expect(blocked_producer_queue.push(1) == QueueStatus::success,
           "setup push must succeed");
    auto blocked_producer = std::async(std::launch::async, [&] {
        return blocked_producer_queue.push(2);
    });
    expect(blocked_producer.wait_for(20ms) == std::future_status::timeout,
           "push must block while the queue is full");
    blocked_producer_queue.close();
    expect(blocked_producer.wait_for(1s) == std::future_status::ready &&
               blocked_producer.get() == QueueStatus::closed,
           "close must wake a blocked producer");

    int drained = 0;
    expect(blocked_producer_queue.try_pop(drained) == QueueStatus::success && drained == 1,
           "items queued before close must remain drainable");
    expect(blocked_producer_queue.try_pop(drained) == QueueStatus::closed,
           "drained closed queue must report closed");
    expect(blocked_producer_queue.try_push(3) == QueueStatus::closed,
           "closed queue must reject new items");
}
