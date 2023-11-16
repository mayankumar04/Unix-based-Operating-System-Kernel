#ifndef _KERNEL_H_
#define _KERNEL_H_
#include "elf.h"
#include "future.h"
#include <coroutine>

Future<int> kernelMain(void);
extern Ext2* fs;

#endif
