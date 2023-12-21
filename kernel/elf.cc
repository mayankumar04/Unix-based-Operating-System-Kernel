#include "elf.h"

#include "debug.h"
#include "process.h"
#include "vmm.h"

bool ELF::check_program_headers(Shared<Node> file, uint32_t num_program_headers, ProgramHeader* phs) {
    // NOTE : could check that no headers overlap with each other
    for (uint32_t i = 0; i < num_program_headers; i++) {
        ProgramHeader& ph = phs[i];

        if (ph.type == ELF_PH_TYPE_LOAD) {
            // NOTE : could check that there is enough space for a small stack as well
            // check space
            if (!VMM::is_region_in_user_mem(ph.vaddr, ph.vaddr + ph.memsz)) {
                return false;
            }

            // check file
            if (ph.offset + ph.filesz < ph.offset || ph.offset + ph.filesz >= file->size_in_bytes()) {
                return false;
            }
        }
    }

    return true;
}

void ELF::handle_program_headers(Shared<Node> file, uint32_t num_program_headers, ProgramHeader* phs) {
    for (uint32_t i = 0; i < num_program_headers; i++) {
        ProgramHeader& ph = phs[i];

        if (ph.type == ELF_PH_TYPE_LOAD) {
            load_program_header(file, ph);
        }
    }
}

void ELF::load_program_header(Shared<Node> file, ProgramHeader& ph) {
    using namespace ProcessManagement;

    char* addr = (char*)ph.vaddr;

    ASSERT(mmap(PCB::current().mmap_tree, (VirtualAddress)addr, ph.memsz,
                Flags::MMAP_FIXED | Flags::MMAP_F_UNALGN | Flags::MMAP_F_TRUNC | Flags::MMAP_REAL | Flags::MMAP_RW | Flags::MMAP_USER,
                file, ph.offset, ph.filesz) == addr);
}

uint32_t ELF::load(Shared<Node> file) {
    // read elf header
    ElfHeader elf_header{};
    uint32_t bytes_read = file->read_all(0, sizeof(elf_header), (char*)&elf_header);
    if (bytes_read != sizeof(elf_header)) {
        return 0;
    }

    // check elf
    unsigned char* magic = elf_header.magic;
    if (magic[0] != '\x7f' || magic[1] != 'E' || magic[2] != 'L' || magic[3] != 'F') {
        return 0;
    }

    // check entry is safe
    if (!VMM::is_region_in_user_mem(elf_header.entry, elf_header.entry + 1)) {
        return 0;
    }

    // check 32 bit
    if (elf_header.cls != 1) {
        return 0;
    }

    // get program headers and check them
    if (sizeof(ProgramHeader) != elf_header.program_header_entry_size) {
        return 0;
    }

    ProgramHeader* program_headers = new ProgramHeader[elf_header.num_program_headers];
    uint32_t nbyte_read = file->read_all(elf_header.program_header_off, elf_header.num_program_headers * sizeof(ProgramHeader), (char*)program_headers);
    bool valid = nbyte_read == elf_header.num_program_headers * sizeof(ProgramHeader);

    if (!valid || !check_program_headers(file, elf_header.num_program_headers, program_headers)) {
        delete[] program_headers;
        return 0;
    }

    handle_program_headers(file, elf_header.num_program_headers, program_headers);

    delete[] program_headers;
    return elf_header.entry;
}
