#include "semaphore.h"

void Semaphore::up() {
    lock.lock();
    auto e = waiting.remove();
    if (e != nullptr) {
        ASSERT(count == 0);
        impl::ready_queue.add(e);
        lock.unlock();
    } else {
        count += 1;
        lock.unlock();
    }
}