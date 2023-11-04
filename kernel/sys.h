#ifndef _SYS_H_
#define _SYS_H_
#include "stdint.h"
#include "idt.h"
#include "debug.h"
#include "machine.h"
#include "events.h"
#include "physmem.h"
#include "elf.h"
#include "vmm.h"
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

#endif