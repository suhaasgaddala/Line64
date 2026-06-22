#include <assert.h>
#include <pthread.h>

enum { consumer_count = 2 };

static pthread_mutex_t mutex;
static int published;
static int payload;
static int observed[consumer_count];

static void* publish(void* unused) {
    (void)unused;
    assert(pthread_mutex_lock(&mutex) == 0);
    payload = 73;
    published = 1;
    assert(pthread_mutex_unlock(&mutex) == 0);
    return NULL;
}

static void* consume(void* raw_index) {
    const long index = (long)raw_index;
    for (;;) {
        assert(pthread_mutex_lock(&mutex) == 0);
        if (published) {
            observed[index] = payload;
            assert(pthread_mutex_unlock(&mutex) == 0);
            return NULL;
        }
        assert(pthread_mutex_unlock(&mutex) == 0);
    }
}

int main(void) {
    assert(pthread_mutex_init(&mutex, NULL) == 0);

    pthread_t producer;
    pthread_t consumers[consumer_count];
    assert(pthread_create(&producer, NULL, publish, NULL) == 0);
    assert(pthread_create(&consumers[0], NULL, consume, (void*)0) == 0);
    assert(pthread_create(&consumers[1], NULL, consume, (void*)1) == 0);
    assert(pthread_join(producer, NULL) == 0);
    assert(pthread_join(consumers[0], NULL) == 0);
    assert(pthread_join(consumers[1], NULL) == 0);

    assert(observed[0] == 73);
    assert(observed[1] == 73);
    assert(pthread_mutex_destroy(&mutex) == 0);
}
