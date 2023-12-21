#ifndef _INIT_H_
#define _INIT_H_

#include "stdint.h"

extern "C" void kernelInit(void);
extern "C" uint32_t pickKernelStack(void);
extern "C" void graphicsInit(void);
extern "C" void windowInit(void);

extern bool onHypervisor;

#endif
