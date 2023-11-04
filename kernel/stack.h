#ifndef MAYANK04_STACK_H
#define MAYANK04_STACK_H
#include "vector.h"

template <typename T>
class stack{
    int top_index;
    vector<T> v;

public:
    stack(){
        top_index = -1;
    }

    void push(T t){
        v.push_back(t);
        ++top_index;
    }

    T pop(){
        ASSERT(top_index >= 0);
        T result = v.get(top_index);
        --top_index;
        return result;
    }

    T top(){
        ASSERT(top_index >= 0);
        return v.get(top_index);
    }

    bool empty(){
        return top_index < 0;
    }

    int size(){
        return top_index < 0 ? 0 : top_index + 1;
    }
};

#endif