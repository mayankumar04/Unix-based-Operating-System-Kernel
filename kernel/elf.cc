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
            if(p_head->vaddr < 0x80000000 || p_head->vaddr >= 0xF0000000 || p_head->memsz < p_head->filesz || p_head->memsz >= 0xF0000000 - p_head->vaddr){
                delete elf_head;
                delete[] buffer;
                delete p_head;
                return 0;
            }
            char* new_buffer = new char[p_head->filesz];
            if(file->read_all(p_head->offset, p_head->filesz, new_buffer) < p_head->filesz || !empty_update(p_head->vaddr, p_head->memsz)){
                delete elf_head;
                delete[] buffer;
                delete p_head;
                delete[] new_buffer;
                return 0;
            }
            memcpy((char*)(p_head->vaddr), new_buffer, p_head->filesz);
            delete[] new_buffer;
        }
        delete p_head;
    }
    delete elf_head;
    delete[] buffer;
    return ans;
}

bool ELF::empty_update(uint32_t addr, uint32_t size){
    map_range* curr = pcbs.mine()->empty_list;
    while(curr != nullptr){
        if(curr->size < size){
            curr = curr->next;
            continue;
        }
        if(curr->addr == addr && curr->size == size){
            if(curr->prev == nullptr && curr->next == nullptr)
                pcbs.mine()->empty_list = nullptr;
            else if(curr->prev == nullptr){
                curr->next->prev = nullptr;
                pcbs.mine()->empty_list = curr->next;
            }else if(curr->next == nullptr)
                curr->prev->next = nullptr;
            else{
                curr->prev->next = curr->next;
                curr->next->prev = curr->prev;
            }
            delete curr;
            return true;
        }else if(curr->addr == addr && curr->size > size){
            curr->addr = addr + size;
            curr->size -= size;
            return true;
        }else if(curr->addr < addr && addr - curr->addr == curr->size - size){
            curr->size -= size;
            return true;
        }else if(curr->addr < addr && addr - curr->addr < curr->size - size){
            map_range* inserted = new map_range{addr + size, (curr->size - size) - (addr - curr->addr), curr, curr->next, false, nullptr, 0};
            curr->size = addr - curr->addr;
            if(curr->next != nullptr)
                curr->next->prev = inserted;
            curr->next = inserted;
            return true;
        }
        curr = curr->next;
    }
    return false;
}

bool ELF::empty_add(uint32_t addr, uint32_t size){
    if(addr < 0x80000000 || addr >= 0xF0000000 || size >= 0xF0000000 - addr)
        return false;
    if(pcbs.mine()->empty_list == nullptr){
        pcbs.mine()->empty_list = new map_range{addr, size, nullptr, nullptr, false, nullptr, 0};
        return true;
    }
    map_range* curr = pcbs.mine()->empty_list;
    if(addr < curr->addr && size <= curr->addr - addr){
        if(size == curr->addr - addr){
            curr->addr = addr;
            curr->size += size;
        }
        else{
            curr->prev = new map_range{addr, size, nullptr, curr, false, nullptr, 0};
            pcbs.mine()->empty_list = curr->prev;
        }
        return true;
    }
    while(curr->next != nullptr){
        if(addr > curr->addr && addr < curr->next->addr){
            if(addr - curr->addr == curr->size && curr->next->addr - addr == size){
                curr->size += size + curr->next->size;
                map_range* temp = curr->next;
                curr->next = curr->next->next;
                if(curr->next != nullptr)
                    curr->next->prev = curr;
                delete temp;
                return true;
            }else if(addr - curr->addr == curr->size){
                curr->size += size;
                return true;
            }else if(curr->next->addr - addr == size){
                curr->next->addr = addr;
                curr->next->size += size;
                return true;
            }else if(curr->addr + curr->size < addr && addr + size < curr->next->addr){
                map_range* temp = new map_range{addr, size, curr, curr->next, false, nullptr, 0};
                curr->next->prev = temp;
                curr->next = temp;
                return true;
            }else return false;
        }
        curr = curr->next;
    }
    if(addr - curr->addr == curr->size)
        curr->size += size;
    else
        curr->next = new map_range{addr, size, curr, nullptr, false, nullptr, 0};
    return true;
}