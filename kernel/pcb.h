#ifndef MAYANK04_PCB_H
#define MAYANK04_PCB_H
#include "stdint.h"
#include "future.h"
#include "stack.h"
#include "ext2.h"
#include "semaphore.h"
#include "kernel.h"

struct file{
    Node* file;
    bool read, write, stdout = false;
    uint32_t offset = 0;
};

struct map_range{
    uint32_t addr, size;
    map_range *prev, *next;
    bool loaded = false;
    file* fd;
    uint32_t offset;
};

struct pcb {
    uint32_t pd;
    uint32_t regs[13];
    uint32_t back_regs[13];
    Future<int> f;
    stack<pcb*> s;
    Semaphore** sems;
    map_range* empty_list, *mmap;
    void* handler;
    Node* cwd;
    file** fd;

    pcb(){
        sems = new Semaphore*[100];
        fd = new file*[10];
        for(uint32_t i = 0; i < 100; ++i)
            sems[i] = nullptr;
        fd[0] = new file{nullptr, false, false};
        fd[1] = new file{nullptr, false, true, true};
        fd[2] = new file{nullptr, false, true, true};
        for(uint32_t i = 3; i < 10; ++i)
            fd[i] = nullptr;
        handler = nullptr;
        mmap = nullptr;
        empty_list = new map_range{0x80000000, 0x6FF00000, nullptr, nullptr, false, nullptr, 0};
        cwd = fs->root;
    }

    ~pcb(){
        delete[] sems;
        delete[] fd;
        while(mmap != nullptr){
            map_range* temp = mmap;
            mmap = mmap->next;
            delete temp;
        }
        while(empty_list != nullptr){
            map_range* temp = empty_list;
            empty_list = empty_list->next;
            delete temp;
        }
    };

    void print(){
        map_range* curr = mmap;
        while(curr != nullptr){
            Debug::printf("MMAP BEGIN: %x, END: %x\n", curr->addr, curr->addr + curr->size);
            curr = curr->next;
        }
        curr = empty_list;
        while(curr != nullptr){
            Debug::printf("EMPTY LIST BEGIN: %x, END: %x\n", curr->addr, curr->addr + curr->size);
            curr = curr->next;
        }
    }
};

#endif