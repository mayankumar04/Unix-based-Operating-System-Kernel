#include "events.h"

namespace impl {
Queue<Event, SpinLock> ready_queue{};

struct PQEntry {
    uint32_t const at;
    Event* e;
    PQEntry* next = nullptr;
    PQEntry(uint32_t const at, Event* e) : at(at), e(e) {}
};

struct PQ {
    PQEntry* head = nullptr;
    SpinLock lock{};  // is this wise? no because add is O(n)

    void add(uint32_t const at, Event* e) {
        auto pqe = new PQEntry(at, e);
        lock.lock();
        auto p = head;
        auto pprev = &head;
        while (p != nullptr) {
            if (p->at > at) break;
            pprev = &p->next;
            p = p->next;
        }
        pqe->next = p;
        *pprev = pqe;
        lock.unlock();
    }

    Event* remove_if_ready() {
        lock.lock();
        auto pqe = head;
        if (pqe == nullptr) {
            lock.unlock();
            return nullptr;
        }
        if (pqe->at <= Pit::jiffies) {
            auto e = pqe->e;
            head = pqe->next;
            lock.unlock();
            delete pqe;
            return e;
        }
        lock.unlock();
        return nullptr;
    }
} pq;

void timed(const uint32_t at, Event* e) {
    pq.add(at, e);
}

PerCPU<Event*> pending_event{};

void manage_pending() {
    if (pending_event.mine() != nullptr) {
        Event* e = pending_event.mine();
        pending_event.mine() = nullptr;
        delete e;
    }
}

}  // namespace impl

void event_loop() {
    using namespace impl;

    manage_pending();

    // Debug::printf("| core#%d entring event_loop\n", SMP::me());
    while (true) {
        while (true) {
            auto e = pq.remove_if_ready();
            if (e == nullptr) break;
            ready_queue.add(e);
        }
        auto e = ready_queue.remove();
        if (e == nullptr) {
            pause();
        } else {
            pending_event.mine() = e;
            e->doit();
            manage_pending();
        }
    }
}
