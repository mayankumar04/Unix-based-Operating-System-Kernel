#include "sys.h"

extern Ext2* fs;
PerCPU<pcb*> pcbs;

extern "C" int sysHandler(uint32_t eax, uint32_t *frame) {
    auto userEsp = (uint32_t*) frame[11];
    // auto userEip = frame[8];
    switch(eax) {
        case 0:{
            exit(userEsp[1]);
        }
        case 1:{
            if(userEsp[2] < 0x80000000 || userEsp[2] >= 0xF0000000){
                frame[7] = -1;
                return 69;
            }
            if(userEsp[2] + userEsp[3] > 0xF0000000){
                frame[7] = -1;
                return 69;
            }
            auto p = (char*) userEsp[2];
            for(uint32_t i = 0; i < userEsp[3]; ++i)
                Debug::printf("%c", p[i]);
            frame[7] = userEsp[3];
            return 69;
        }
        case 2:{
            uint32_t* og_pd = (uint32_t*) getCR3();
            uint32_t* copy_pd = (uint32_t*) PhysMem::alloc_frame();
            for(int i = 0; i < 1024; ++i) {
                if(i < 32) copy_pd[i] = og_pd[i];
                else{
                    uint32_t og_pt = og_pd[i];
                    if(og_pt & 1){
                        uint32_t* copy_pt = (uint32_t*) PhysMem::alloc_frame();
                        copy_pd[i] = ((uint32_t) copy_pt) | (og_pt & 0xFFF);
                        uint32_t* og_pt_ptr = (uint32_t*) (og_pt & 0xFFFFF000);
                        for(int j = 0; j < 1024; ++j) {
                            if((i == 960 && j == 0) || (i == 1019 && (j == 0 || j == 512))) copy_pt[j] = og_pt_ptr[j];
                            else{
                                uint32_t og_page = og_pt_ptr[j];
                                if(og_page & 1){
                                    uint32_t copy_page = PhysMem::alloc_frame();
                                    memcpy((char *) copy_page, (char *) (og_page & 0xFFFFF000), 4096);
                                    copy_pt[j] = copy_page | (og_page & 0xFFF);
                                }
                            }
                        }
                    }
                }
            }
            pcb* child_pcb = new pcb();
            child_pcb->pd = (uint32_t) copy_pd;
            memcpy((char*)(child_pcb->regs), (char*)frame, 52);
            child_pcb->regs[7] = 0;
            pcbs.mine()->s.push(child_pcb);
            go([child_pcb]() {
                vmm_on(child_pcb->pd);
                pcbs.mine() = child_pcb;
                restart(child_pcb->regs);
            });
            frame[7] = 1;
            return 69;
        }
        case 7:{
            Debug::shutdown();
        }
        case 9:
        case 1000:{
            uint32_t prev = 1, tracker = 1;
            auto curr = fs->root;
            auto path = (char*) userEsp[1];
            if(userEsp[1] < 0x80000000 || userEsp[1] >= 0xF0000000){
                frame[7] = -1;
                return 69;
            }
            while(path[tracker] != '\0'){
                if(path[tracker] == '/'){
                    char* file_name = new char[tracker - prev + 1];
                    file_name[tracker - prev] = '\0';
                    for(uint32_t i = prev; i < tracker; ++i){
                        file_name[i - prev] = path[i];
                    }
                    if(!curr->is_dir()){
                        delete[] file_name;
                        frame[7] = -1;
                        return 69;
                    }
                    curr = fs->find(curr, file_name);
                    if(curr == nullptr){
                        delete[] file_name;
                        frame[7] = -1;
                        return 69;
                    }
                    delete[] file_name;
                    prev = tracker + 1;
                }
                ++tracker;
            }
            auto file_name = new char[tracker - prev + 1];
            file_name[tracker - prev] = '\0';
            for(uint32_t i = prev; i < tracker; ++i)
                file_name[i - prev] = path[i];
            curr = fs->find(curr, file_name);
            if(curr == nullptr || !curr->is_file()){
                delete[] file_name;
                frame[7] = -1;
                return 69;
            }
            delete[] file_name;
            uint32_t args = 0;
            while(userEsp[args + 2] != 0){
                if(userEsp[args + 2] < 0x80000000 || userEsp[args + 2] >= 0xF0000000){
                    frame[7] = -1;
                    return 69;
                }
                ++args;
            }
            uint32_t total = 0;
            for(uint32_t i = 2; i < args + 2; ++i){
                char* curr_arg = (char*) userEsp[i];
                uint32_t j = 1;
                while(curr_arg[j - 1] != '\0')
                    ++j;
                total += j;
            }
            uint32_t N = 0xF0000000 - total;
            N -= N % 4;
            if(N - 4 * (args + 3) < 0x80000000){
                frame[7] = -1;
                return 69;
            }
            uint32_t* arg_pointers = new uint32_t[args + 1];
            arg_pointers[args] = 0;
            char* all_args = new char[total];
            if(args > 0){
                prev = 0;
                for (uint32_t i = 2; i < args + 2; ++i) {
                    auto curr_arg = (char *) userEsp[i];
                    uint32_t j = 0;
                    while (curr_arg[j] != '\0') {
                        all_args[prev + j] = curr_arg[j];
                        ++j;
                    }
                    all_args[prev + j] = '\0';
                    arg_pointers[i - 2] = N + prev;
                    prev += j + 1;
                }
            }
            uint32_t old_pd = getCR3();
            VMM::per_core_init();
            delete_file(old_pd);
            uint32_t entry = ELF::load(curr);
            if(entry < 0x80000000 || entry >= 0xF0000000){
                frame[7] = -1;
                return 69;
            }
            memcpy((char*) N, all_args, total);
            delete[] all_args;
            N -= 4 * (args + 1);
            memcpy((char*) N, (char*)arg_pointers, 4 * (args + 1));
            delete[] arg_pointers;
            uint32_t* last = new uint32_t[2];
            last[1] = N;
            last[0] = args;
            N -= 8;
            memcpy((char*) N, (char*) last, 8);
            delete[] last;
            switchToUser(entry, N, 0);
        }
        case 999:{
            pcb* curr_pcb = pcbs.mine();
            if(curr_pcb->s.empty()){
                frame[7] = -1;
                return 69;
            }
            curr_pcb->pd = getCR3();
            memcpy((char*)(curr_pcb->regs), (char*)frame, 52);
            vmm_on(VMM::kernel_map);
            pcbs.mine()->s.pop()->f.get([curr_pcb, frame](uint32_t error_code){
                vmm_on(curr_pcb->pd);
                pcbs.mine() = curr_pcb;
                curr_pcb->regs[7] = error_code;
                restart(curr_pcb->regs);
            });
            event_loop();
        }
        case 998:{
            pcb* curr_pcb = pcbs.mine();
            curr_pcb->pd = getCR3();
            memcpy((char*)(curr_pcb->regs), (char*)frame, 52);
            vmm_on(VMM::kernel_map);
            go([curr_pcb]{
                vmm_on(curr_pcb->pd);
                pcbs.mine() = curr_pcb;
                restart(curr_pcb->regs);
            });
            event_loop();
        }
        case 1001:{
            Semaphore* sm = new Semaphore(userEsp[1]);
            frame[7] = (uint32_t) sm;
            return 69;
        }
        case 1002:{
            ((Semaphore*) userEsp[1])->up();
            break;
        }
        case 1003:{
            pcb* curr_pcb = pcbs.mine();
            curr_pcb->pd = getCR3();
            memcpy((char*)(curr_pcb->regs), (char*)frame, 52);
            Semaphore* temp = (Semaphore*) userEsp[1];
            vmm_on(VMM::kernel_map);
            temp->down([curr_pcb]{
                vmm_on(curr_pcb->pd);
                pcbs.mine() = curr_pcb;
                restart(curr_pcb->regs);
            });
            event_loop();
        }
        default:{
            Debug::panic("syscall %d\n", eax);
        }
    }
    frame[7] = 0;
    return 69;
}

void SYS::init(void) {
    IDT::trap(48,(uint32_t)sysHandler_,3);
}

void exit(uint32_t error_code){
    pcbs.mine()->f.set(error_code);
    uint32_t old_pd = getCR3();
    VMM::per_core_init();
    vmm_on(VMM::kernel_map);
    delete_file(old_pd);
    delete pcbs.mine();
    event_loop();
}

void delete_file(uint32_t old_pd){
    uint32_t* pd_ptr = (uint32_t*) old_pd;
    for(int i = 32; i < 960; ++i){
        uint32_t pt = pd_ptr[i];
        if(pt & 1){
            uint32_t* pt_ptr = (uint32_t*)(pt & 0xFFFFF000);
            for(int j = 0; j < 1024; ++j)
                if(pt_ptr[j] & 1)
                    PhysMem::dealloc_frame(pt_ptr[j] & 0xFFFFF000);
            PhysMem::dealloc_frame((uint32_t) pt_ptr);
        }
    }
    PhysMem::dealloc_frame(old_pd);
}