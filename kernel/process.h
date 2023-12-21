#pragma once

#include "events.h"
#include "file.h"
#include "future.h"
#include "genericds.h"
#include "semaphore.h"
#include "vmm.h"

// rename to ProcessManagement
namespace ProcessManagement {

using namespace VMM;
using namespace Generic;
using namespace UserFileIO;

// ------------------- declarations --------------------

struct RegisterState;
class ExitHandle;
struct PCB;
class Process;

/**
 * names the stuff that gets pushed when doing an interrupt
 * this includes the things pushed by "int" and "pusha"
 * assuming "pusha" is after "int"
 */
struct RegisterState {
    // low address = most recent push
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_int;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t error_code;
    uint32_t eip;
    uint32_t cs;
    uint32_t flags;
    uint32_t esp_user;
    uint32_t ss;
    // high address = least recent push

    inline void clear();

    inline RegisterState();
    inline RegisterState(const RegisterState& rhs);
    inline RegisterState(RegisterState&& rhs);

    inline RegisterState& operator=(RegisterState const& rhs);
    inline RegisterState& operator=(RegisterState&& rhs);
};

/**
 * a handle to a child
 */
class ExitHandle {
    Future<int> status;

   public:
    inline ExitHandle();

    inline void exit(int rc);
    inline void wait(Process me);
};

/**
 * a handle to a semaphore
 */
class SemaphoreHandle {
    Semaphore sem;

   public:
    inline SemaphoreHandle(uint32_t n);

    inline void up();
    inline void down(Process me);
};

/**
 * the process control block
 */
struct PCB {
    RegisterState regs;
    LinkedList<RegisterState, NoLock> regs_stack;  // NOTE : probably need to deep copy this
    RBTree<MMAPBlock*, NoLock>* mmap_tree;
    uint32_t id;
    uint32_t active_tui;
    Shared<ExitHandle> exit_status;
    LinkedList<Shared<ExitHandle>, NoLock> children;

    Shared<SemaphoreHandle> sems[100];

    Shared<Node> working_directory;
    OpenFile user_files[10];

    void (* handler)(int, unsigned);
    bool in_signal_handler;

    inline PCB();
    inline PCB(RegisterState regs,
               RBTree<MMAPBlock*, NoLock>* mmap_tree);
    inline ~PCB() = default;

    /**
     * pushes the current state onto the state stack
     */
    inline void push_state();

    /**
     * pops a new state off the state stack
     */
    inline void pop_state();

    /**
     * saves the register state into this pcb's first register state
     * panics if no states are available
     */
    inline void save_state(RegisterState* regs);

    /**
     * resumes the process with the register state
     * does not return
     */
    inline void resume();

    /**
     * gets the current pcb
     */
    inline static PCB& current();

    // placement new operators to call constructor
    void* operator new(size_t sz, void* p);
};

/**
 * Represents a process
 */
class Process {
   public:
    PageDir pd;

    inline Process();
    inline Process(PageDir pd);

    /**
     * schedules this process to run
     */
    template <typename Work>
    inline void schedule(Work callback_before_resume) const;
    inline void schedule() const;

    /**
     * get the physical pcb of the process
     */
    inline PCB& pcb_phys() const;

    inline bool operator==(const Process& rhs);

    /**
     * gets the current process
     */
    inline static Process current();

    /**
     * changes the current process to the given process, returning the old process
     */
    inline static Process change(Process process_to_switch_to);

    /**
     * creates another process like the given one, returning the new process
     */
    static Process create_like(Process process_to_copy);

    /**
     * destroys the given process and switches to the default_kernel_process
     * if the given process is the current process. the default_kernel_process
     * is not destroyable
     */
    static void destroy(Process process_to_delete);
};

// ------------- globals -------------

constexpr VirtualAddress VA_PROCESS = VA_USER_END;

// the next pid
extern Atomic<uint32_t> next_pid;

// the preempt flag
extern PerCPU<bool> preempt_flag;

// the default process (the template)
extern Process default_kernel_process;

// the per core kernel processes
extern PerCPU<Process> default_kernel_per_core_process;

// ------------- interface -------------

/**
 * initializes the global kernel process. expects VMM::global_init() is done
 */
extern void global_init();

/**
 * loads the kernel process for each core. expects VMM::per_core_init() is done
 */
extern void per_core_init();

/**
 * switches vmm, blocks the current active process, and runs the callback
 * after the callback is done, sits in event loop if wait is true
 * work(Process)
 */
template <typename Work>
void block(Work callback);

/**
 * switches vmm, blocks the current active process, and calls
 */
extern void yield();

/**
 * switches to user mode with the given register state
 */
extern void user_mode(RegisterState* regs);

/**
 * handles the timer
 */
extern void handle_timer(RegisterState* regs);

/**
 * syncs a barrier
 */
extern void block_and_sync(Shared<Barrier>& trigger);

/**
 * switches vmm, possibly yields the current active process, or returns the value,
 * depending on the flag status
 */
inline int return_or_yield(int ret_val);

/**
 * switches vmm, possibly yields the current active process, or resumes it
 * depending on the flag status
 */
inline void resume_or_yield();

// ------------------- definitions --------------------

// +++ RegisterState

inline void RegisterState::clear() {
    // zero the fields
    bzero(this, sizeof(*this));

    // we need to inialize some stuff
    esp_int = (uint32_t)&eip;
    flags = 0x200;
    cs = userCS;
    ss = userSS;
}

inline RegisterState::RegisterState() {
    clear();
}

inline RegisterState::RegisterState(const RegisterState& rhs) {
    memcpy(this, &rhs, sizeof(*this));
    esp_int = (uint32_t)&eip;
}

inline RegisterState::RegisterState(RegisterState&& rhs) {
    memcpy(this, &rhs, sizeof(*this));
    esp_int = (uint32_t)&eip;
    rhs.clear();
}

inline RegisterState& RegisterState::operator=(RegisterState const& rhs) {
    memcpy(this, &rhs, sizeof(*this));
    esp_int = (uint32_t)&eip;
    return *this;
}

inline RegisterState& RegisterState::operator=(RegisterState&& rhs) {
    memcpy(this, &rhs, sizeof(*this));
    esp_int = (uint32_t)&eip;
    rhs.clear();
    return *this;
}

// +++ ExitHandle

inline ExitHandle::ExitHandle() : status() {}

inline void ExitHandle::exit(int rc) {
    status.set(rc);
}

inline void ExitHandle::wait(Process me) {
    status.get([me](int rc) {
        me.schedule([rc] {
            PCB::current().regs.eax = rc;
        });
    });
}

// +++ SemaphoreHandle

inline SemaphoreHandle::SemaphoreHandle(uint32_t n) : sem(n) {}

inline void SemaphoreHandle::up() {
    sem.up();
}

inline void SemaphoreHandle::down(Process me) {
    sem.down([me] {
        me.schedule();
    });
}

// +++ PCB

inline PCB::PCB() : PCB(RegisterState(), nullptr) {}

inline PCB::PCB(RegisterState regs, RBTree<MMAPBlock*, NoLock>* mmap_tree) : regs(regs),
                                                                             regs_stack(RegisterState()),
                                                                             mmap_tree(mmap_tree),
                                                                             id(next_pid.add_fetch(1)),
                                                                             exit_status(Shared<ExitHandle>::make()),
                                                                             children(Shared<ExitHandle>::NUL),
                                                                             sems(),
                                                                             working_directory(Shared<Node>::NUL),
                                                                             user_files(),
                                                                             handler(nullptr),
                                                                             in_signal_handler(false) {}

inline void PCB::push_state() {
    regs_stack.add_left(regs);
    regs.clear();
}

inline void PCB::pop_state() {
    regs = regs_stack.remove_left();
}

inline void PCB::save_state(RegisterState* regs) {
    this->regs = *regs;
}

inline void PCB::resume() {
    user_mode(&regs);
}

inline PCB& PCB::current() {
    return *(PCB*)(VA_PROCESS);
}

// +++ Process

inline Process::Process() : Process(PageNum::bad()) {}
inline Process::Process(PageDir pd) : pd(pd) {}

template <typename Work>
inline void Process::schedule(Work callback_before_resume) const {
    go([me = *this, callback_before_resume] {
        Process::change(me);
        callback_before_resume();
        PCB::current().resume();
    });
}

inline void Process::schedule() const {
    schedule([] {});
}

inline PCB& Process::pcb_phys() const {
    PageNum process_vpn = page_num(VA_PROCESS);

    PageEntry& pde = pd[process_vpn.pdi()];
    ASSERT(pde.flags().is(Flags::PRESENT));

    PageTable pt = pde.ppn();
    PageEntry& pte = pt[process_vpn.pti()];
    ASSERT(pte.flags().is(Flags::PRESENT));

    return *(PCB*)pte.address();
}

inline bool Process::operator==(const Process& rhs) {
    return pd.ppn() == rhs.pd.ppn();
}

inline Process Process::current() {
    return current_cr3();
}

inline Process Process::change(Process process_to_switch_to) {
    return exchange_cr3(process_to_switch_to.pd);
}

// ------------- interface

// go back to kernel vmm, use scoping to deconstruct the process as this blocks
template <typename Work>
void block(Work callback) {
    {
        Process process = Process::change(default_kernel_per_core_process.mine());
        callback(process);
    }
    event_loop();
}

inline int return_or_yield(int ret_val) {
    if (preempt_flag.mine()) {
        preempt_flag.mine() = false;
        PCB::current().regs.eax = ret_val;
        yield();
    }
    return ret_val;
}

inline void resume_or_yield() {
    if (preempt_flag.mine()) {
        preempt_flag.mine() = false;
        yield();
    }
    PCB::current().resume();
}

}  // namespace ProcessManagement