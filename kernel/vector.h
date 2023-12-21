#include "debug.h"

template <typename T>
class vector{
    T* arr;
    int cap, curr;

public:
    vector(){
        arr = new T[1];
        cap = 1;
        curr = 0;
    }

    //~vector(){
    //    delete[] arr;
    //}

    void push_back(T t){
        if(curr == cap){
            T* temp = new T[2 * cap];
            for(int i = 0; i < cap; ++i)
                temp[i] = arr[i];
            //delete[] arr;
            cap *= 2;
            arr = temp;
        }
        arr[curr] = t;
        ++curr;
    }

    void insert(T t, int index){
        ASSERT(index <= curr && index >= 0);
        if(curr + 1 <= cap){
            for(int i = curr; i > index; --i)
                arr[i] = arr[i - 1];
            arr[index] = t;
        }else{
            T* temp = new T[2 * cap];
            for(int i = 0; i < cap; ++i){
                if(i < index)
                    temp[i] = arr[i];
                else
                    temp[i + 1] = arr[i];
            }
            temp[index] = t;
            //delete[] arr;
            cap *= 2;
            arr = temp;
        }
        ++curr;
    }

    T get(int index){
        //Debug::printf("BAD INDEX: %d", index);
        ASSERT(index < curr && index >= 0);
        return arr[index];
    }

    void delete_back(){
        --curr;
    }

    void erase(int index){
        ASSERT(index < curr && index >= 0);
        for(int i = index + 1; i < curr; ++i)
            arr[i - 1] = arr[i];
        --curr;
    }

    int size(){
        return curr;
    }
};