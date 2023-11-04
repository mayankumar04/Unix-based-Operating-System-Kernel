#ifndef _KERNEL_H_
#define _KERNEL_H_
#include "debug.h"
#include "ide.h"
#include "ext2.h"
#include "elf.h"
#include "machine.h"
#include "libk.h"
#include "config.h"
#include "future.h"
#include "sys.h"
#include "pcb.h"
#include <coroutine>

Future<int> kernelMain(void);

#endif
