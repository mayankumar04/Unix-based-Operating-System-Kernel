#ifndef _INIT_H_
#define _INIT_H_
#include "stdint.h"
#include "u8250.h"
#include "machine.h"
#include "idt.h"
#include "crt.h"
#include "tss.h"
#include "physmem.h"
#include "vmm.h"

extern "C" void kernelInit(void);
extern "C" uint32_t kernelPickStack(void);

extern bool onHypervisor;

#endif
