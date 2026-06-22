#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>

enum { capacity = 2, worker_count = 3 };

struct cell {
    _Atomic size_t sequence;
    int value;
};

struct producer_state {
    int value;
    size_t sequence;
};

struct consumer_state {
    int value;
    size_t sequence;
};

static struct cell cells[capacity];
static _Atomic size_t enqueue_position;
static _Atomic size_t dequeue_position;

static int try_push(int value, size_t* logical_sequence) {
    size_t position = atomic_load_explicit(&enqueue_position, memory_order_relaxed);
    struct cell* cell;

    for (;;) {
        cell = &cells[position & (capacity - 1)];
        const size_t sequence = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        if (sequence == position) {
            if (atomic_compare_exchange_weak_explicit(
                    &enqueue_position, &position, position + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (sequence < position) {
            return 0;
        } else {
            position = atomic_load_explicit(&enqueue_position, memory_order_relaxed);
        }
    }

    cell->value = value;
    *logical_sequence = position + 1;
    atomic_store_explicit(&cell->sequence, position + 1, memory_order_release);
    return 1;
}

static int try_pop(int* value, size_t* logical_sequence) {
    size_t position = atomic_load_explicit(&dequeue_position, memory_order_relaxed);
    struct cell* cell;

    for (;;) {
        cell = &cells[position & (capacity - 1)];
        const size_t sequence = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        if (sequence == position + 1) {
            if (atomic_compare_exchange_weak_explicit(
                    &dequeue_position, &position, position + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (sequence < position + 1) {
            return 0;
        } else {
            position = atomic_load_explicit(&dequeue_position, memory_order_relaxed);
        }
    }

    *value = cell->value;
    *logical_sequence = position + 1;
    atomic_store_explicit(&cell->sequence, position + capacity, memory_order_release);
    return 1;
}

static void* produce(void* raw_state) {
    struct producer_state* state = raw_state;
    while (!try_push(state->value, &state->sequence)) {
    }
    return NULL;
}

static void* consume(void* raw_state) {
    struct consumer_state* state = raw_state;
    while (!try_pop(&state->value, &state->sequence)) {
    }
    return NULL;
}

int main(void) {
    for (size_t index = 0; index < capacity; ++index) {
        atomic_init(&cells[index].sequence, index);
    }
    atomic_init(&enqueue_position, 0);
    atomic_init(&dequeue_position, 0);

    struct producer_state producer_state[worker_count] = {
        {11, 0}, {22, 0}, {33, 0}
    };
    struct consumer_state consumer_state[worker_count] = {
        {0, 0}, {0, 0}, {0, 0}
    };
    pthread_t producers[worker_count];
    pthread_t consumers[worker_count];

    for (size_t index = 0; index < worker_count; ++index) {
        assert(pthread_create(&producers[index], NULL, produce, &producer_state[index]) == 0);
        assert(pthread_create(&consumers[index], NULL, consume, &consumer_state[index]) == 0);
    }
    for (size_t index = 0; index < worker_count; ++index) {
        assert(pthread_join(producers[index], NULL) == 0);
        assert(pthread_join(consumers[index], NULL) == 0);
    }

    for (size_t left = 0; left < worker_count; ++left) {
        assert(consumer_state[left].value == 11 ||
               consumer_state[left].value == 22 ||
               consumer_state[left].value == 33);
        for (size_t right = left + 1; right < worker_count; ++right) {
            assert(producer_state[left].sequence != producer_state[right].sequence);
            assert(consumer_state[left].sequence != consumer_state[right].sequence);
            assert(consumer_state[left].value != consumer_state[right].value);
        }
    }
}
