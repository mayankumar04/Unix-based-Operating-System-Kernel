#include "sys.h"

extern Ext2* fs;
PerCPU<pcb*> pcbs;

extern "C" int sysHandler(uint32_t eax, uint32_t *frame) {
    auto userEsp = (uint32_t*) frame[11];
    // auto userEip = frame[8];
    switch(eax) {

        // EXIT
        case 0:{
            exit(userEsp[1]);
        }

        // PRINT
        case 1:{
            if(userEsp[2] < 0x80000000 || userEsp[2] >= 0xF0000000 || userEsp[3] >= 0xF0000000 - userEsp[2]){
                frame[7] = -1;
                return 69;
            }
            char* p = (char*) userEsp[2];
            //Debug::printf("CURR PD: %x ", getCR3());
            for(uint32_t i = 0; i < userEsp[3]; ++i)
                Debug::printf("%c", p[i]);
            frame[7] = userEsp[3];
            return 69;
        }

        // FORK
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
            memcpy((char*)(pcbs.mine()->regs), (char*)frame, 52);
            memcpy((char*)(child_pcb->back_regs), (char*)(pcbs.mine()->back_regs), 52);
            child_pcb->regs[7] = 0;
            pcbs.mine()->regs[7] = 1;
            for(uint32_t i = 0; i < 100; ++i)
                child_pcb->sems[i] = pcbs.mine()->sems[i];
            child_pcb->handler = nullptr;
            if(pcbs.mine()->mmap != nullptr){
                child_pcb->mmap = new map_range{pcbs.mine()->mmap->addr, pcbs.mine()->mmap->size, nullptr, nullptr};
                child_pcb->mmap->loaded = pcbs.mine()->mmap->loaded;
                map_range* curr_child = child_pcb->mmap, *curr = pcbs.mine()->mmap->next;
                while(curr != nullptr){
                    curr_child->next = new map_range{curr->addr, curr->size, curr_child, nullptr};
                    curr_child->next->loaded = curr->loaded;
                    curr_child = curr_child->next;
                    curr = curr->next;
                }
            }
            if(pcbs.mine()->empty_list != nullptr){
                child_pcb->empty_list = new map_range{pcbs.mine()->empty_list->addr, pcbs.mine()->empty_list->size, nullptr, nullptr};
                map_range* curr_child = child_pcb->empty_list, *curr = pcbs.mine()->empty_list->next;
                while(curr != nullptr){
                    curr_child->next = new map_range{curr->addr, curr->size, curr_child, nullptr};
                    curr_child = curr_child->next;
                    curr = curr->next;
                }
            }
            pcbs.mine()->s.push(child_pcb);
            go([child_pcb]() {
                vmm_on(child_pcb->pd);
                pcbs.mine() = child_pcb;
                restart(child_pcb->regs);
            });
            frame[7] = 1;
            return 69;
        }

        // SHUTDOWN
        case 7:{
            Debug::shutdown();
        }

        // YIELD
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

        // JOIN
        case 999:{
            pcb* curr_pcb = pcbs.mine();
            if(curr_pcb->s.empty()){
                frame[7] = -1;
                return 69;
            }
            pcb* top_child = pcbs.mine()->s.pop();
            curr_pcb->pd = getCR3();
            memcpy((char*)(curr_pcb->regs), (char*)frame, 52);
            vmm_on(VMM::kernel_map);
            top_child->f.get([top_child, curr_pcb, frame](uint32_t error_code){
                vmm_on(curr_pcb->pd);
                pcbs.mine() = curr_pcb;
                curr_pcb->regs[7] = error_code;
                delete top_child;
                restart(curr_pcb->regs);
            });
            event_loop();
        }

        // EXECL
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
            pcb* old_pcb = pcbs.mine();
            pcbs.mine() = new pcb();
            pcbs.mine()->pd = getCR3();
            memcpy((char*)(pcbs.mine()->regs), (char*)(old_pcb->regs), 52);
            memcpy((char*)(pcbs.mine()->back_regs), (char*)(old_pcb->back_regs), 52);
            uint32_t entry = ELF::load(curr);
            if(entry < 0x80000000 || entry >= 0xF0000000){
                vmm_on(old_pd);
                delete pcbs.mine();
                pcbs.mine() = old_pcb;
                frame[7] = -1;
                return 69;
            }
            delete_file(old_pd);
            delete old_pcb;
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

        // SEM
        case 1001:{
            for(uint32_t i = 0; i < 100; ++i)
                if(pcbs.mine()->sems[i] == nullptr){
                    pcbs.mine()->sems[i] = new Semaphore(userEsp[1]);
                    frame[7] = i;
                    return 69;
                }
            frame[7] = -1;
            return 69;
        }

        // SEM-UP
        case 1002:{
            if(userEsp[1] >= 100 || pcbs.mine()->sems[userEsp[1]] == nullptr){
                frame[7] = -1;
                return 69;
            }
            pcbs.mine()->sems[userEsp[1]]->up();
            break;
        }

        // SEM-DOWN
        case 1003:{
            if(userEsp[1] >= 100 || pcbs.mine()->sems[userEsp[1]] == nullptr){
                frame[7] = -1;
                return 69;
            }
            pcbs.mine()->pd = getCR3();
            uint32_t* temp = new uint32_t[13];
            memcpy((char*)temp, (char*)frame, 52);
            vmm_on(VMM::kernel_map);
            pcb* curr_pcb = pcbs.mine();
            pcbs.mine()->sems[userEsp[1]]->down([curr_pcb, temp]{
                vmm_on(curr_pcb->pd);
                pcbs.mine() = curr_pcb;
                memcpy((char*)(curr_pcb->regs), (char*)temp, 52);
                delete[] temp;
                curr_pcb->regs[7] = 0;
                restart(curr_pcb->regs);
            });
            event_loop();
        }

        // SIGNAL
        case 1004:{
            pcbs.mine()->handler = (void*) userEsp[1];
            break;
        }

        // MMAP
        case 1005:{
            uint32_t addr = userEsp[1], size = userEsp[2];
            if(addr % 4096 != 0 || size % 4096 != 0)
                break;
            if(size == 0){
                frame[7] = 69;
                return 69;
            }
            if(addr == 0){
                map_range* curr = pcbs.mine()->empty_list;
                while(curr != nullptr){
                    if(curr->size <= size){
                        frame[7] = curr->addr;
                        pcbs.mine()->mmap = new map_range{curr->addr, size, nullptr, pcbs.mine()->mmap};
                        pcbs.mine()->mmap->next->prev = pcbs.mine()->mmap;
                        ELF::empty_update(curr->addr, size);
                        return 69;
                    }
                    curr = curr->next;
                }
            }else{
                if(!ELF::empty_update(addr, size))
                    break;
                pcbs.mine()->mmap = new map_range{addr, size, nullptr, pcbs.mine()->mmap};
                pcbs.mine()->mmap->next->prev = pcbs.mine()->mmap;
                frame[7] = addr;
                return 69;
            }
            break;
        }

        // SIG-RETURN
        case 1006:{
            if(pcbs.mine()->handler == nullptr)
                memcpy((char*)(pcbs.mine()->regs), (char*)frame, 52);
            else
                memcpy((char*)(pcbs.mine()->regs), (char*)(pcbs.mine()->back_regs), 52);
            restart(pcbs.mine()->regs);
        }

        // SEM CLOSE
        case 1007:{
            if(userEsp[1] >= 100 || pcbs.mine()->sems[userEsp[1]] == nullptr){
                frame[7] = -1;
                return 69;
            }
            delete pcbs.mine()->sems[userEsp[1]];
            pcbs.mine()->sems[userEsp[1]] = nullptr;
            break;
        }

        // UNMAP
        case 1008:{
            uint32_t addr = userEsp[1];
            if(addr < 0x80000000 || addr >= 0xF0000000){
                frame[7] = -1;
                return 69;
            }
            map_range* curr = pcbs.mine()->mmap;
            while(curr != nullptr){
                if(addr >= curr->addr && addr - curr->addr < curr->size){
                    if(!ELF::empty_add(curr->addr, curr->size))
                        break;
                    if(curr->loaded){
                        uint32_t* pd = (uint32_t*) getCR3();
                        for(uint32_t i = curr->addr; i - curr->addr < curr->size; i += 4096){
                            uint32_t pt = pd[i >> 22];
                            if(pt & 1){
                                uint32_t* pt_ptr = (uint32_t*)(pt & 0xFFFFF000);
                                if(pt_ptr[(i << 10) >> 22] & 1){
                                    PhysMem::dealloc_frame(pt_ptr[(i << 10) >> 22] & 0xFFFFF000);
                                    pt_ptr[(i << 10) >> 22] = 0;
                                }
                            }
                        }
                        vmm_on(getCR3());
                    }
                    if(curr->prev == nullptr && curr->next == nullptr)
                        pcbs.mine()->mmap = nullptr;
                    else if(curr->prev == nullptr)
                        pcbs.mine()->mmap = curr->next;
                    else if(curr->next == nullptr)
                        curr->prev->next = nullptr;
                    else{
                        curr->prev->next = curr->next;
                        curr->next->prev = curr->prev;
                    }
                    delete curr;
                    frame[7] = 0;
                    return 69;
                }
                curr = curr->next;
            }
            frame[7] = -1;
            return 69;
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
    vmm_on(VMM::kernel_map);
    delete_file(old_pd);
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