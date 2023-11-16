#ifndef _SYS_H_
#define _SYS_H_
#include "debug.h"
#include "events.h"
#include "physmem.h"
#include "elf.h"
#include "smp.h"
#include "pcb.h"
#include "semaphore.h"

extern PerCPU<pcb*> pcbs;

class SYS {
public:
    static void init(void);
    static void do_exit(int rc);
};

void exit(uint32_t error_code);
void delete_file(uint32_t old_pd);
Node* find_node(char* path);
bool verify_range(uintptr_t va_, uintptr_t* frame);

#endif