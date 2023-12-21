#include "flags.h"

const Flags Flags::PRESENT = Flags(0x1);
const Flags Flags::READ_WRITE = Flags(0x2);
const Flags Flags::USER_SUPERVISOR = Flags(0x4);
const Flags Flags::KERNEL = Flags(0x3);
const Flags Flags::ALL = Flags(0x7);

const Flags Flags::MMAP_REAL = Flags(0x1);
const Flags Flags::MMAP_RW = Flags(0x2);
const Flags Flags::MMAP_USER = Flags(0x4);
const Flags Flags::MMAP_SHARED = Flags(0x8);
const Flags Flags::MMAP_FIXED = Flags(0x10);
const Flags Flags::MMAP_F_UNALGN = Flags(0x20);
const Flags Flags::MMAP_F_TRUNC = Flags(0x40);

const Flags Flags::USER_FILE_READ = Flags(0x1);
const Flags Flags::USER_FILE_WRITE = Flags(0x2);