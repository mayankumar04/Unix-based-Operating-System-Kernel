#pragma once

#include "debug.h"
#include "semaphore.h"
#include "stdint.h"

// A barrier that can be reused. Behavior is identical
// to Barrier otherwise
class ReusableBarrier {
    Atomic<uint32_t> n;
    uint32_t og_n;
    Semaphore sem;
    SpinLock guard;

   public:
    ReusableBarrier(uint32_t count) : n(count), og_n(count), sem(0), guard() {}

    ReusableBarrier(const ReusableBarrier&) = delete;

    template <typename Work>
    void sync(const Work& work) {
        sem.down([work] {
            work();
        });

        LockGuard g{guard};
        if (n.add_fetch(-1) == 0) {
            for (uint32_t i = 0; i < og_n; i++) {
                sem.up();
            }
            n.set(og_n);
        }
    }

    void sync() {
        sync([]() {});
    }
};

class Barrier {
    ReusableBarrier barrier;

   public:
    // Create a barrier of size "n"
    explicit Barrier(const uint32_t n) : barrier(n) {}

    // schedules "work" to run when exactly "n" calls to "sync" have been made
    // calling "sync" less than "n" times strands the work.
    // calling "sync" more than "n" times has undefined behavior
    template <typename Work>
    void sync(const Work& work) {
        barrier.sync(work);
    }

    void sync() {
        barrier.sync();
    }
};
