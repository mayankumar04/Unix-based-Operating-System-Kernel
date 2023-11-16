#include "vmm.h"
#include "config.h"
#include "sys.h"

extern PerCPU<pcb*> pcbs;

namespace VMM {

    uint32_t shared_pt, APIC, kernel_map;

    void global_init() {
        shared_pt = PhysMem::alloc_frame();
        uint32_t shared_frame = PhysMem::alloc_frame();
        uint32_t* shared_pt_ptr = (uint32_t*) shared_pt;
        uint32_t* shared_frame_ptr = (uint32_t*) shared_frame;
        for(uint32_t i = 0; i < 1024; ++i)
            shared_frame_ptr[i] = 0;
        for(uint32_t i = 0; i < 1024; ++i)
            shared_pt_ptr[i] = 0;
        shared_pt_ptr[0] = shared_frame | 0b111;
        shared_pt |= 0b111;
        APIC = PhysMem::alloc_frame();
        uint32_t* APIC_ptr = (uint32_t*) APIC;
        for(uint32_t i = 0; i < 1024; ++i)
            APIC_ptr[i] = 0;
        APIC_ptr[0] = kConfig.ioAPIC | 0b10011;
        APIC_ptr[512] = kConfig.localAPIC | 0b10011;
        APIC |= 0b10011;

        // assigning the kernel-specific identity map
        kernel_map = PhysMem::alloc_frame();
        uint32_t* kernel_map_ptr = (uint32_t*) kernel_map;
        for(uint32_t i = 0; i < 1024; ++i)
            kernel_map_ptr[i] = 0;
        kernel_map_ptr[960] = shared_pt;
        kernel_map_ptr[1019] = APIC;
        for(uint32_t i = 0; i < 32; ++i){
            uint32_t pt = PhysMem::alloc_frame();
            uint32_t* pt_ptr = (uint32_t*) pt;
            for(uint32_t j = 0; j < 1024; ++j)
                pt_ptr[j] = (i << 22) + (j << 12) + 0b11;
            kernel_map_ptr[i] = pt | 0b11;
        }
    }

    void per_core_init() {
        uint32_t pd = PhysMem::alloc_frame();
        uint32_t* pd_ptr = (uint32_t*) pd;
        for(uint32_t i = 0; i < 1024; ++i)
            pd_ptr[i] = 0;
        pd_ptr[960] = shared_pt;
        pd_ptr[1019] = APIC;
        for(uint32_t i = 0; i < 32; ++i){
            uint32_t pt = PhysMem::alloc_frame();
            uint32_t* pt_ptr = (uint32_t*) pt;
            for(uint32_t j = 0; j < 1024; ++j)
                pt_ptr[j] = (i << 22) + (j << 12) + 0b11;
            pd_ptr[i] = pt | 0b11;
        }
        vmm_on(pd);
    }
}

void signal_handler(uintptr_t va_, uintptr_t* frame){
    if(pcbs.mine()->handler == nullptr){
        ((uint32_t*) 0xF0000800)[0] = va_;
        exit(139);
    }else{
        pcb* curr_pcb = pcbs.mine();
        curr_pcb->pd = getCR3();
        memcpy((char*)(curr_pcb->back_regs), (char*)(curr_pcb->regs), 52);
        uint32_t userEsp = frame[12] - 140;
        userEsp -= userEsp % 16;
        ((uint32_t*) userEsp)[1] = 1;
        ((uint32_t*) userEsp)[2] = va_;
        switchToUser((uint32_t) pcbs.mine()->handler, userEsp, 0);
    }
}

void load_mmap(uint32_t va_, map_range* curr, uint32_t load_addr){
    if(curr->fd == nullptr || !curr->fd->read)
        return;
    uint32_t true_offset = curr->offset + (va_ - (va_ % 4096) - curr->addr);
    char* buffer = new char[4096];
    memcpy(buffer, (char*) load_addr, 4096);
    if(curr->fd->file->read_all(true_offset, 4096, buffer) < 0)
        Debug::panic("*** THE FILE SYSTEM COULD NOT READ FROM THE FILE\n");
    memcpy((char*) load_addr, buffer, 4096);
    delete[] buffer;
}

extern "C" void vmm_pageFault(uintptr_t va_, uintptr_t* frame){
    memcpy((char*)(pcbs.mine()->regs), (char*)frame, 32);
    memcpy((char*)(pcbs.mine()->regs + 8), (char*)(frame + 9), 20);
    if(va_ < 0x80000000 || va_ >= 0xF0000000)
        signal_handler(va_, frame);
    map_range* curr = pcbs.mine()->empty_list;
    while(curr != nullptr){
        if(va_ >= curr->addr && va_ - curr->addr < curr->size)
            signal_handler(va_, frame);
        curr = curr->next;
    }
    curr = pcbs.mine()->mmap;
    bool mmap_loading = false;
    uint32_t load_addr = 0;
    while(curr != nullptr){
        if(va_ >= curr->addr && va_ - curr->addr < curr->size){
            mmap_loading = true;
            curr->loaded = true;
            break;
        }
        curr = curr->next;
    }
    uint32_t* pd = (uint32_t*) getCR3(), x = va_ >> 22, y = (va_ << 10) >> 22, pt = pd[x];
    if(pt & 1){
        uint32_t* pt_ptr = (uint32_t*) (pt & 0xFFFFF000);
        pt_ptr[y] = PhysMem::alloc_frame() | 0b111;
    }else{
        pt = PhysMem::alloc_frame();
        uint32_t* pt_ptr = (uint32_t*) pt;
        load_addr = PhysMem::alloc_frame();
        pt_ptr[y] = load_addr | 0b111;
        pd[x] = pt | 0b111;
    }
    if(mmap_loading)
        load_mmap(va_, curr, load_addr);
}