#pragma once

#include "atomic.h"
#include "heap.h"
#include "stdint.h"

template <typename T>
struct Counted {
    T data;
    Atomic<uint32_t> ref_count{0};

    template <typename... Args>
    inline Counted(Args... args) : data(args...) {}

    Counted(Counted const&) = delete;
    Counted(Counted&&) = delete;
    Counted& operator=(Counted const&) = delete;
    Counted& operator=(Counted&&) = delete;
};

// Smart pointer for a reference-counted object of type T
template <typename T>
class Shared {
    Counted<T>* ptr;

    inline void ref() {
        if (ptr != nullptr) {
            ptr->ref_count.add_fetch(1);
        }
    }

    void unref() {
        if (ptr != nullptr) {
            auto n = ptr->ref_count.add_fetch(-1);
            if (n == 0) {
                delete ptr;
            }
        }
    }

    explicit inline Shared(Counted<T>* ptr) : ptr(ptr) {
        ref();
    }

   public:
    static const Shared<T> NUL;

    inline Shared() : ptr(nullptr) {
    }

    inline Shared(Shared<T> const& rhs) : ptr(rhs.ptr) {
        ref();
    }

    inline Shared(Shared<T>&& rhs) : ptr(rhs.ptr) {
        rhs.ptr = nullptr;
    }

    Shared<T>& operator=(Shared<T> const& rhs) {
        if (ptr != rhs.ptr) {
            unref();
            ptr = rhs.ptr;
            ref();
        }
        return *this;
    }

    Shared<T>& operator=(Shared<T>&& rhs) {
        if (ptr != rhs.ptr) {
            unref();
            ptr = rhs.ptr;
            rhs.ptr = nullptr;
        }
        return *this;
    }

    ~Shared() {
        unref();
    }

    void reset() {
        unref();
        ptr = nullptr;
    }

    inline T* operator->() const {
        return &ptr->data;
    }

    inline bool operator==(Shared<T> const& rhs) {
        return ptr == rhs.ptr;
    }

    template <typename... Args>
    inline static Shared<T> make(Args... args) {
        return Shared<T>{new Counted<T>(args...)};
    }
};

template <typename T>
const Shared<T> Shared<T>::NUL= Shared<T>();