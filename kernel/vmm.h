#ifndef _VMM_H_
#define _VMM_H_
#include "stdint.h"
#include "machine.h"
#include "physmem.h"

namespace VMM {

    // Called (on the initial core) to initialize data structures, etc
    extern void global_init();

    // Called on each core to do per-core initialization
    extern void per_core_init();

    extern uint32_t shared_pt, APIC, kernel_map;
}

#endif
