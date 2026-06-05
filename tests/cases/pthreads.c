// Exercises POSIX threads end-to-end on the wasm32-wasip1-threads target.
// This is the first C-sourced test case; the tests/Makefile compiles
// cases/*.c with the wasi-sdk threads sysroot, so the produced module pulls
// in a shared memory and the `wasi:thread-spawn` import that real threading
// requires.
//
// The test pulls on every part of the pthread path that the JIT/host must get
// right:
//   - pthread_create actually spawns and runs the worker (thread-spawn import)
//   - the argument handed to the worker arrives intact
//   - the worker's return value comes back through pthread_join
//   - many threads hammering one counter behind a mutex produce an *exact*
//     total — a lost update (broken shared memory or atomics behind the mutex)
//     shows up as a short count
//
// Each failure mode returns a distinct non-zero code; exit-0 means threads
// ran, synchronized, and observed each other's writes through shared memory.

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#define NUM_THREADS 8
#define ITERS_PER_THREAD 100000

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t counter = 0;

static void* worker(void* arg) {
    uintptr_t id = (uintptr_t)arg;

    for (int i = 0; i < ITERS_PER_THREAD; i++) {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }

    // Hand a per-thread value back so the join path is exercised too, not just
    // the side effect on shared memory. (+1 so thread 0 is distinguishable
    // from a NULL/zero return.)
    return (void*)(id + 1);
}

int main(void) {
    pthread_t threads[NUM_THREADS];

    for (uintptr_t i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, worker, (void*)i) != 0) {
            return 1;
        }
    }

    for (uintptr_t i = 0; i < NUM_THREADS; i++) {
        void* ret = NULL;
        if (pthread_join(threads[i], &ret) != 0) {
            return 2;
        }
        if ((uintptr_t)ret != i + 1) {
            return 3;
        }
    }

    // Exact total proves the mutex serialized the increments across shared
    // memory; a missing/lost update would leave this short.
    if (counter != (uint64_t)NUM_THREADS * ITERS_PER_THREAD) {
        return 4;
    }

    return 0;
}
