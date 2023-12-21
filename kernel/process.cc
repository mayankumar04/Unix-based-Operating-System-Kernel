#include "process.h"

#include "debug.h"
#include "tss.h"
#include "vmm.h"

namespace ProcessManagement {

Atomic<uint32_t> next_pid{0};
PerCPU<bool> preempt_flag{};
Process default_kernel_process{PageNum(PageNum::BAD_NUM)};
PerCPU<Process> default_kernel_per_core_process{};

void* PCB::operator new(size_t sz, void* p) {
    return p;
}

Process Process::create_like(Process process_to_copy) {
    // create the new proc
    Process new_proc = create_page_dir_like(process_to_copy.pd);

    // handle page faults immediately for kernel data (as kernel will just bypass the read only)
    // we need a new page for the current process. we can use the default block list as every PD
    // should have process page
    handle_page_fault(process_to_copy.pcb_phys().mmap_tree, new_proc.pd, VA_PROCESS, true);

    // also force the pagefault on the old proc
    handle_page_fault(process_to_copy.pcb_phys().mmap_tree, process_to_copy.pd, VA_PROCESS, true);

    // initialize PCB members
    new (&new_proc.pcb_phys()) PCB(process_to_copy.pcb_phys().regs,
                                   create_mmap_tree_like(process_to_copy.pcb_phys().mmap_tree));

    return new_proc;
}

void Process::destroy(Process process_to_delete) {
    // don't delete the default process
    if (process_to_delete == default_kernel_process || process_to_delete == default_kernel_per_core_process.mine()) {
        return;
    }

    // save where we are going to end up
    if (Process::current() == process_to_delete) {
        Process::change(default_kernel_per_core_process.mine());
    }

    PCB& process_to_delete_pcb = process_to_delete.pcb_phys();
    RBTree<MMAPBlock*, NoLock>* mmap_tree = process_to_delete_pcb.mmap_tree;
    process_to_delete_pcb.~PCB();

    // destroy the process
    destroy_page_dir(mmap_tree, process_to_delete.pd);
    destroy_mmap_tree(mmap_tree);
}

void global_init() {
    // create the default kernel process
    default_kernel_process = Process(create_page_dir_like(default_page_dir));

    // force the COW behavior
    handle_page_fault(default_mmap_tree, default_kernel_process.pd, VA_PROCESS, true);
    handle_page_fault(default_mmap_tree, default_page_dir, VA_PROCESS, true);

    PCB& default_pcb = default_kernel_process.pcb_phys();

    // initialize PCB members
    new (&default_pcb) PCB(RegisterState(),
                           create_mmap_tree_like(default_mmap_tree));

    // map the default user space and shared frame
    VirtualAddress va_user_stack = VA_USER_PRIVATE_END - (1024 * 1024);
    ASSERT(mmap(default_pcb.mmap_tree, va_user_stack, 1024 * 1024, Flags::MMAP_FIXED | Flags::MMAP_RW | Flags::MMAP_REAL | Flags::MMAP_USER, Shared<Node>(), 0, 0) == (void*)va_user_stack);
    ASSERT(mmap(default_pcb.mmap_tree, VA_USER_SHARED_START, VA_USER_SHARED_END - VA_USER_SHARED_START, Flags::MMAP_FIXED | Flags::MMAP_RW | Flags::MMAP_REAL | Flags::MMAP_USER | Flags::MMAP_SHARED, Shared<Node>(), 0, 0) == (void*)VA_USER_SHARED_START);
    handle_page_fault(default_pcb.mmap_tree, default_kernel_process.pd, VA_USER_SHARED_START, true);

    for (uint32_t i = 0; i < kConfig.totalProcs; i++) {
        default_kernel_per_core_process.forCPU(i) = Process::create_like(default_kernel_process);
    }
}

void per_core_init() {
    Process::change(default_kernel_per_core_process.mine());
}

void yield() {
    block([](Process me) {
        me.schedule();
    });
}

void user_mode(RegisterState* regs) {
    /**
     * let's use inline assembly to do a few simple things:
     * we will first change our stack to the state
     * then we can simply popa and iret
     */
    asm volatile(
        "mov %[regs], %%esp \n"
        "popa \n"
        "add $4, %%esp \n"
        "iret \n"
        :
        : [regs] "m"(regs));
}

void handle_timer(RegisterState* regs) {
    // interrupts should be disabled

    // if we are in kernel, then we shouldn't preempt
    if (regs->cs == kernelCS) {
        preempt_flag.mine() = true;
        return;
    }

    // yield to somewhere else
    PCB::current().save_state(regs);
    block([](Process me) {
        me.schedule();
        sti();
    });
}

void block_and_sync(Shared<Barrier>& trigger) {
    using namespace ProcessManagement;

    // block here
    block([&trigger](Process me) {
        // only allow the schedule of the process when we have safely disconnected from the process PD
        trigger->sync([me] {
            me.schedule();
        });

        // consume the barrier
        trigger = Shared<Barrier>::NUL;
    });
}
}  // namespace ProcessManagement