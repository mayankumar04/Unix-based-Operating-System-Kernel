#pragma once

#include "semaphore.h"
#include "queue.h"

// A bounded buffer is a generalization of a channel
// that has a buffer size "n > 0"
template <typename T>
class BoundedBuffer {
    Semaphore sem_send;
    Semaphore sem_receive;

    struct ValueNode {
        T v;
        ValueNode* next;
        ValueNode(const T& val) : v(val) {}
    };
    Queue<ValueNode, SpinLock> q;

   public:
    // construct a BB with a buffer size of n
    BoundedBuffer(uint32_t n) : sem_send(n), sem_receive(0), q{} {}
    BoundedBuffer(const BoundedBuffer&) = delete;

    // When room is available in the buffer
    //    - put "v" in the next slot
    //    - schedule a call "work()"
    // Returns immediately
    template <typename Work>
    void put(T v, const Work& work) {
        q.add(new ValueNode(v));
        sem_send.down([this, v, work] {
            sem_receive.up();
            work();
        });
    }

    // When room is available in the buffer
    //    - put "v" in the next slot
    // Returns immediately
    void put(T v) {
        put(v, [] {});
    }

    // When the buffer is not empty
    //    - remove the first value "v"
    //    - schedule a call to "work(v)"
    // Returns immediately
    template <typename Work>
    void get(const Work& work) {
        sem_receive.down([this, work] {
            ValueNode* val_node = q.remove();
            sem_send.up();
            work(val_node->v);
            delete val_node;
        });
    }
};
