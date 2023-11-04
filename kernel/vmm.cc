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

extern "C" void vmm_pageFault(uintptr_t va_, uintptr_t* saveState) {
    if(va_ < 0x80000000 || va_ >= 0xF0000000){
        ((uint32_t*) 0xF0000800)[0] = va_;
        exit(139);
    }
    uint32_t *pd = (uint32_t*) getCR3(), x = va_ >> 22, y = (va_ << 10) >> 22, pt = pd[x];
    if(pt & 1){
        uint32_t* pt_ptr = (uint32_t*) (pt & 0xFFFFF000);
        pt_ptr[y] = PhysMem::alloc_frame() | 0b111;
    }else{
        pt = PhysMem::alloc_frame();
        uint32_t* pt_ptr = (uint32_t*) pt;
        pt_ptr[y] = PhysMem::alloc_frame() | 0b111;
        pd[x] = pt | 0b111;
    }

    int N = pcbs.mine()->segments.size();
    for(int i = 0; i < N; ++i){
        load_segment segment = pcbs.mine()->segments.get(i);
        if(va_ >= segment.addr && va_ < segment.addr + segment.size){
            if(pcbs.mine()->file->read_all(segment.offset, segment.size, (char*)segment.addr) < segment.size)
                exit(139);
            pcbs.mine()->segments.erase(i);
            break;
        }
    }
}