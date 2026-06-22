#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>

enum { capacity = 2 };

static int slots[capacity];
static _Atomic size_t head;
static _Atomic size_t tail;
static int consumed;

static int try_push(int value) {
    const size_t position = atomic_load_explicit(&head, memory_order_relaxed);
    if (position - atomic_load_explicit(&tail, memory_order_acquire) == capacity) {
        return 0;
    }

    slots[position % capacity] = value;
    atomic_store_explicit(&head, position + 1, memory_order_release);
    return 1;
}

static int try_pop(int* value) {
    const size_t position = atomic_load_explicit(&tail, memory_order_relaxed);
    if (position == atomic_load_explicit(&head, memory_order_acquire)) {
        return 0;
    }

    *value = slots[position % capacity];
    atomic_store_explicit(&tail, position + 1, memory_order_release);
    return 1;
}

static void* produce(void* unused) {
    (void)unused;
    assert(try_push(42));
    return NULL;
}

static void* consume(void* unused) {
    (void)unused;
    while (!try_pop(&consumed)) {
    }
    return NULL;
}

int main(void) {
    atomic_init(&head, 0);
    atomic_init(&tail, 0);

    pthread_t producer;
    pthread_t consumer;
    assert(pthread_create(&producer, NULL, produce, NULL) == 0);
    assert(pthread_create(&consumer, NULL, consume, NULL) == 0);
    assert(pthread_join(producer, NULL) == 0);
    assert(pthread_join(consumer, NULL) == 0);
    assert(consumed == 42);
}
