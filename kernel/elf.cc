#include "elf.h"
#include "sys.h"

extern PerCPU<pcb*> pcbs;

uint32_t ELF::load(Node* file) {
    char* buffer = new char[52];
    file->read_all(0, 52, buffer);
    ElfHeader* elf_head = new ElfHeader();
    memcpy((char*)elf_head, buffer, 52);
    uint32_t ans = elf_head->entry;
    if(ans < 0x80000000){
        delete elf_head;
        delete[] buffer;
        return 0;
    }
    delete[] buffer;
    if(elf_head->magic0 != 0x7f || elf_head->magic1 != 'E' || elf_head->magic2 != 'L' || elf_head->magic3 != 'F'){
        delete elf_head;
        return 0;
    }
    if(elf_head->cls != 1 || elf_head->encoding != 1 || elf_head->header_version != 1){
        delete elf_head;
        return 0;
    }
    if(elf_head->abi != 0 || elf_head->type != 2 || elf_head->machine != 3 || elf_head->version != 1){
        delete elf_head;
        return 0;
    }
    uint32_t total = elf_head->phnum;
    buffer = new char[32 * total];
    if(file->read_all(elf_head->phoff, 32 * total, buffer) < 32 * total){
        delete elf_head;
        delete[] buffer;
        return 0;
    }
    for(uint32_t i = 0; i < total; ++i){
        ProgramHeader* p_head = new ProgramHeader();
        memcpy((char*)p_head, buffer + 32 * i, 32);
        if(p_head->type == 1){
            if(p_head->vaddr < 0x80000000 || p_head->memsz < p_head->filesz){
                delete elf_head;
                delete[] buffer;
                delete p_head;
                return 0;
            }
            if(ans >= p_head->vaddr && ans < p_head->vaddr + p_head->memsz){
                char* new_buffer = new char[p_head->filesz];
                if(file->read_all(p_head->offset, p_head->filesz, new_buffer) < p_head->filesz){
                    delete elf_head;
                    delete[] buffer;
                    delete p_head;
                    delete[] new_buffer;
                    return 0;
                }
                memcpy((char*)(p_head->vaddr), new_buffer, p_head->filesz);
                delete[] new_buffer;
            }else
                pcbs.mine()->segments.push_back(load_segment{p_head->offset, p_head->filesz, p_head->vaddr});
        }
        delete p_head;
    }
    delete elf_head;
    delete[] buffer;
    pcbs.mine()->file = file;
    return ans;
}