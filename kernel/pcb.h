#ifndef MAYANK04_PCB_H
#define MAYANK04_PCB_H
#include "stdint.h"
#include "future.h"
#include "stack.h"
#include "ext2.h"

struct load_segment{
    uint32_t offset;
    uint32_t size;
    uint32_t addr;
};

struct pcb {
    uint32_t pd;
    uint32_t regs[13];
    Future<int> f;
    stack<pcb*> s;
    vector<load_segment> segments;
    Node* file;
};

#endif