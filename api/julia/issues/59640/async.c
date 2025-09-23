#include <pthread.h>
#include <stdlib.h>

// Thread entry point
static void* async_thread(void* arg) {
    void (*f)(void) = (void (*)(void)) arg;
    f();
    return NULL;
}

static pthread_t thread;

// Public API

void async(void (*f)(void)) {
    pthread_create(&thread, NULL, async_thread, f);
}

void sync(void) {
    pthread_join(thread, NULL);
}
