#include "sys.h"
#include "debug.h"
#include "elf.h"
#include "events.h"
#include "filesystem.h"
#include "idt.h"
#include "libk.h"
#include "machine.h"
#include "physmem.h"
#include "process.h"
#include "stdint.h"
#include "vmm.h"
#include "keyboard.h"

extern "C" int sysHandler(SYS::Call::EAX eax, ProcessManagement::RegisterState* regs) {
    return SYS::Call::handle_syscall(eax, regs);
}

// reads and returns a 4 byte param
template <typename T>
inline static T& get_param(void* user_esp, uint32_t param_num) {
    return (T&)(((uint32_t*)user_esp)[param_num + 1]);
}

int SYS::Call::handle_syscall(EAX eax, RegisterState* regs) {
    // save the state
    PCB::current().save_state(regs);

    uint32_t* user_esp = (uint32_t*)PCB::current().regs.esp_user;

    // BUG : neeed to add checks that the incoming data is from user mem

    switch (eax) {

        case EXIT: {
            uint32_t status = get_param<uint32_t>(user_esp, 0);
            exit(status);
            return -1;
        }

        case FORK: {
            return return_or_yield(fork());
        }

        case SHUTDOWN: {
            shutdown();
            return -1;
        }

        case YIELD: {
            yield();
            return -1;
        }

        case JOIN: {
            join();
            return -1;
        }

        case EXECL1:
        case EXECL2: {
            // FIXME : should probably put a maximum arg length on this
            // copy program path
            char* program_path = get_param<char*>(user_esp, 0);
            uint32_t program_path_len = K::strlen(program_path);
            char* program_path_kernel = new char[program_path_len + 1];
            K::strcpy(program_path_kernel, program_path);

            // copy args
            char** args = &get_param<char*>(user_esp, 1);
            uint32_t arg_count;
            for (arg_count = 0; args[arg_count] != 0; arg_count++) {
                // count
            }
            char** args_kernel = new char*[arg_count + 1];
            for (uint32_t i = 0; i < arg_count; i++) {
                uint32_t arg_len = K::strlen(args[i]);
                char* arg_kernel = new char[arg_len + 1];
                K::strcpy(arg_kernel, args[i]);
                args_kernel[i] = arg_kernel;
            }
            args_kernel[arg_count] = nullptr;

            // get return value
            uint32_t ret_val = execl(program_path_kernel, args_kernel);

            // clean up if we failed. execl is responsible for cleanup on success
            delete[] program_path_kernel;
            for (uint32_t i = 0; i < arg_count; i++) {
                delete[] args_kernel[i];
            }
            delete[] args_kernel;

            return return_or_yield(ret_val);
        }

        case GETCWD:{
            char* buf = (char*)get_param<uint32_t>(user_esp, 0);
            uint32_t size = get_param<uint32_t>(user_esp, 1);
            return return_or_yield(getcwd(buf, size));
        }

        case SEM: {
            uint32_t n = get_param<uint32_t>(user_esp, 0);
            return return_or_yield(sem(n));
        }

        case UP: {
            uint32_t sem_desc = get_param<uint32_t>(user_esp, 0);
            return return_or_yield(up(sem_desc));
        }

        case DOWN: {
            uint32_t sem_desc = get_param<uint32_t>(user_esp, 0);
            return return_or_yield(down(sem_desc));
        }

        case SIMPLE_SIGNAL: {
            void (*handler)(int, unsigned) = get_param<void (*)(int, unsigned)>(user_esp, 0);
            SYS::Call::simple_signal(handler);
            return return_or_yield(0);
        }

        case SIMPLE_MMAP: {
            void* addr = get_param<void*>(user_esp, 0);
            uint32_t size = get_param<uint32_t>(user_esp, 1);
            int fd = get_param<int>(user_esp, 2);
            uint32_t offset = get_param<uint32_t>(user_esp, 3);
            return return_or_yield((uint32_t)simple_mmap(addr, size, fd, offset));
        }

        case SIGRETURN: {
            return return_or_yield(sigreturn());
        }

        case SEM_CLOSE: {
            uint32_t sem_desc = get_param<uint32_t>(user_esp, 0);
            return return_or_yield(sem_close(sem_desc));
        }

        case SIMPLE_MUNMAP: {
            void* addr = get_param<void*>(user_esp, 0);
            return return_or_yield(simple_munmap(addr));
        }

        case CHDIR: {
            // copy program path
            char* file_path = get_param<char*>(user_esp, 0);
            uint32_t file_path_len = K::strlen(file_path);
            char* file_path_kernel = new char[file_path_len + 1];
            K::strcpy(file_path_kernel, file_path);
            chdir(file_path_kernel);
            delete file_path_kernel;
            return return_or_yield(0);
        }

        case TUI: {
            int ret_val = tui();
            return return_or_yield(ret_val);
        }

        case SET_TUI: {
            int tui_id = get_param<int>(user_esp, 0);
            int ret_val = set_tui(tui_id);
            return return_or_yield(ret_val);
        }

        case OPEN: {
            // copy program path
            char* file_path = get_param<char*>(user_esp, 0);
            uint32_t file_path_len = K::strlen(file_path);
            char* file_path_kernel = new char[file_path_len + 1];
            K::strcpy(file_path_kernel, file_path);
            int ret_val = open(file_path_kernel);
            delete file_path_kernel;
            return return_or_yield(ret_val);
        }

        case CLOSE: {
            int fd = get_param<int>(user_esp, 0);
            return return_or_yield(close(fd));
        }

        case LEN: {
            int fd = get_param<int>(user_esp, 0);
            return return_or_yield(len(fd));
        }

        case READ: {
            int fd = get_param<int>(user_esp, 0);
            void* buffer = get_param<void*>(user_esp, 1);
            size_t len = get_param<size_t>(user_esp, 2);
            return return_or_yield(read(fd, buffer, len));
        }

        case WRITE1:
        case WRITE2: {
            int fd = get_param<int>(user_esp, 0);
            void* buffer = get_param<void*>(user_esp, 1);
            size_t len = get_param<size_t>(user_esp, 2);
            return return_or_yield(write(fd, buffer, len));
        }

        case PIPE: {
            int* write_fd = get_param<int*>(user_esp, 0);
            int* read_fd = get_param<int*>(user_esp, 1);
            return return_or_yield(pipe(write_fd, read_fd));
        }
        case DUP2:
        case DUP: {
            int fd = get_param<int>(user_esp, 0);
            return return_or_yield(dup(fd));
        }
        case GETCH:
        {
            return getChar(); 
        }

        default:
            Debug::panic("syscall %d (%x, %x, %x)\n", eax, user_esp[0], user_esp[1], user_esp[2]);
    }

    return 0;
}

void SYS::init(void) {
    IDT::trap(48, (uint32_t)sysHandler_, 3);
}

void SYS::Call::exit(int rc) {
    using namespace VMM;
    using namespace ProcessManagement;

    block([rc](Process me) {
        me.pcb_phys().exit_status->exit(rc);
        Process::destroy(me);
    });
}

int SYS::Call::fork() {
    using namespace VMM;
    using namespace ProcessManagement;

    // save the state is in pcb for direct to transfer to the child
    // we are forking, so we should create a new child process
    // no need to save the registers because they should be copied through COW
    Process child_process = Process::create_like(Process::current());

    // update PCBs
    PCB& child_pcb = child_process.pcb_phys();
    for (uint32_t i = 0; i < 100; i++) {
        child_pcb.sems[i] = PCB::current().sems[i];
    }
    PCB::current().children.add_left(child_pcb.exit_status);

    child_pcb.working_directory = PCB::current().working_directory;
    for (uint32_t i = 0; i < 10; i++) {
        child_pcb.user_files[i] = PCB::current().user_files[i];
    }

    // BUG need to copy the state stack??

    // schedule child
    child_pcb.regs.eax = 0;
    child_process.schedule();

    return 1;
}

void SYS::Call::shutdown() {
    FileSystem::close();
    Debug::shutdown();
}

void SYS::Call::yield() {
    ProcessManagement::yield();
}

void SYS::Call::join() {
    using namespace ProcessManagement;

    block([=](Process me) {
        Shared<ExitHandle> child_handle = me.pcb_phys().children.remove_left();

        // check if there is no children, in which case we just return -1
        if (child_handle == Shared<ExitHandle>()) {
            me.schedule([]() {
                PCB::current().regs.eax = -1;
            });
        } else {
            child_handle->wait(me);
        }
    });
}

int SYS::Call::execl(char* program_path, char** args) {
    using namespace VMM;
    using namespace ProcessManagement;

    // get program
    Shared<Node> program = FileSystem::find_by_path(PCB::current().working_directory, program_path);
    if (program == Shared<Node>()) {
        return -1;
    }

    // create new process cuz be about to change memory possibly
    Process new_process = Process::create_like(default_kernel_process);
    Process old_process = Process::change(new_process);

    // load program (assumes args have been copied to kernel space)
    uint32_t entry = ELF::load(program);

    // failed
    if (entry == 0) {
        Process::change(old_process);
        Process::destroy(new_process);
        return -1;
    }

    // NOTE: probably better ot put the checks in a separate function

    // copy data from old process
    PCB& old_pcb = old_process.pcb_phys();
    PCB::current().exit_status = old_pcb.exit_status;
    old_pcb.children.transfer_left(PCB::current().children);

    PCB::current().working_directory = old_pcb.working_directory;
    for (uint32_t i = 0; i < 10; i++) {
        PCB::current().user_files[i] = old_pcb.user_files[i];
    }

    Process::destroy(old_process);

    void* user_esp = (void*)VMM::VA_USER_PRIVATE_END;
    user_esp = SYS::Helper::setup_initial_user_stack((void*)user_esp, args);

    // cleanup since we succeeded
    delete[] program_path;
    for (uint32_t i = 0; args[i] != nullptr; i++) {
        delete[] args[i];
    }
    delete[] args;

    // have to call destructor manually because we don't return from here
    program.reset();

    PCB::current().regs.clear();
    PCB::current().regs.eip = entry;
    PCB::current().regs.esp_user = (uint32_t)user_esp;

    resume_or_yield();

    return 0;
}

int SYS::Call::getcwd(char* buf, unsigned size){
    
    if(buf == nullptr) { //is this the correct null check?
        size = (size == 0)?1:size;//replace 1 with cwd size later
        buf = (char*)malloc(size);
    }
    //check if cwd size > size allocaed, return -1 if so
    //otherwise copy cwd into buf and return 
    return -1;
}

int SYS::Call::sem(unsigned n) {
    for (uint32_t i = 0; i < 100; i++) {
        if (PCB::current().sems[i] == Shared<SemaphoreHandle>::NUL) {
            PCB::current().sems[i] = Shared<SemaphoreHandle>::make(n);
            return i;
        }
    }
    return -1;
}

int SYS::Call::up(unsigned sem_desc) {
    if (sem_desc < 0 || sem_desc >= 100 || PCB::current().sems[sem_desc] == Shared<SemaphoreHandle>::NUL) {
        return -1;
    }
    PCB::current().sems[sem_desc]->up();
    return 0;
}

int SYS::Call::down(unsigned sem_desc) {
    if (sem_desc < 0 || sem_desc >= 100 || PCB::current().sems[sem_desc] == Shared<SemaphoreHandle>::NUL) {
        return -1;
    }
    PCB::current().regs.eax = 0;
    block([=](Process me) {
        me.pcb_phys().sems[sem_desc]->down(me);
    });
    return -1;
}

void SYS::Call::simple_signal(void (*handler)(int, unsigned)) {
    // NOTE : maybe need to check the handler to see if it a valid addr??
    PCB::current().handler = handler;
}

void* SYS::Call::simple_mmap(void* addr, unsigned size, int fd, unsigned offset) {
    VirtualAddress va = (VirtualAddress)addr;
    uint32_t length = (uint32_t)size;
    uint32_t file_offset = (uint32_t)offset;

    // check argument veracity
    if (page_down(va) != va ||
        page_down(length) != length ||
        (!is_region_in_user_mem(va, va + length) && va != 0) ||
        page_down(file_offset) != file_offset) {
        return 0;
    }

    // check if we are first fitting or fixed mapping
    Flags mmap_flags = va != 0 ? Flags::MMAP_FIXED : 0;
    mmap_flags = mmap_flags | Flags::MMAP_REAL | Flags::MMAP_RW | Flags::MMAP_USER;

    // check if we are mapping a file or not
    if (fd == -1) {
        return mmap(PCB::current().mmap_tree, va, length, mmap_flags, Shared<Node>::NUL, 0, 0);
    } else {
        if (!SYS::Helper::is_valid_fd(fd)) {
            return 0;
        }

        // BUG : need to check the type of the file

        OpenFile open_file = PCB::current().user_files[fd];
        Shared<Node> node = ((NodeFile*)(open_file.uf->ptr))->node;

        return mmap(PCB::current().mmap_tree, va, length, mmap_flags, node, file_offset, length);
    }
}

int SYS::Call::sigreturn() {
    // make sure we are in a signal handler
    if (!PCB::current().in_signal_handler) {
        return -1;
    }
    PCB::current().pop_state();
    PCB::current().in_signal_handler = false;
    resume_or_yield();
    return -1;
}

int SYS::Call::sem_close(int sem_desc) {
    if (sem_desc < 0 || sem_desc >= 100 || PCB::current().sems[sem_desc] == Shared<SemaphoreHandle>::NUL) {
        return -1;
    }
    PCB::current().sems[sem_desc] = Shared<SemaphoreHandle>::NUL;
    return 0;
}

int SYS::Call::simple_munmap(void* addr) {
    VirtualAddress va = (VirtualAddress)addr;
    if (!is_region_in_user_mem(va, va + 1)) {
        return -1;
    }
    if (munmap_containing_block(PCB::current().mmap_tree, Process::current().pd, va)) {
        return 0;
    }
    return -1;
}

void SYS::Call::chdir(char* path) {
    Shared<Node> cwd = PCB::current().working_directory;
    PCB::current().working_directory = FileSystem::find_by_path(cwd, path);
}

int SYS::Call::tui() {
    int fd = SYS::Helper::get_next_fd();
    if(fd == -1) {
        return -1;
    }
    PCB::current().user_files[fd]= OpenFile(Shared<UserFileContainer>::make(new TUIFile()), Flags::USER_FILE_READ | Flags::USER_FILE_WRITE);
    return fd;
}

bool SYS::Call::set_tui(int tuifd) {
    if(SYS::Helper::is_valid_fd(tuifd)) {
        PCB::current().active_tui = tuifd;
        //Debug::printf("PCB : %x\n", PCB::current());
        //Debug::printf("The active tui is set to this: %d\n", PCB::current().active_tui);
        return 1;
    }
    return 0;
}

// TODO: Implement the TextUI open part; how do we save as a node? Ctrl+S handler??
int SYS::Call::open(const char* path) {
    int fd = SYS::Helper::get_next_fd();
    if (fd == -1) {
        return -1;
    }

    Shared<Node> cwd = PCB::current().working_directory;

    // search for the path
    Shared<Node> node = FileSystem::find_by_path(cwd, path);

    // check if the node is nul
    if (node == Shared<Node>::NUL) {
        return -1;
    }

    // deref if it is a symbolic link
    if (node->is_symlink()) {
        // search for the path
        char* symbol = new char[node->size_in_bytes() + 100];
        node->get_symbol(symbol);
        symbol[node->size_in_bytes()] = 0;

        node = FileSystem::find_by_path(cwd, symbol);

        // check if the node is nul
        if (node == Shared<Node>::NUL) {
            return -1;
        }
    }

    PCB::current().user_files[fd] = OpenFile(Shared<UserFileContainer>::make(new NodeFile(node)),
                                             Flags::USER_FILE_READ | Flags::USER_FILE_WRITE);

    return fd;
}

int SYS::Call::close(int fd) {
    if (!SYS::Helper::is_valid_fd(fd)) {
        return -1;
    }

    PCB::current().user_files[fd] = OpenFile();

    return 0;
}

int SYS::Call::len(int fd) {
    if (!SYS::Helper::is_valid_fd(fd)) {
        return -1;
    }

    return PCB::current().user_files[fd].len();
}

ssize_t SYS::Call::read(int fd, void* buffer, size_t len) {
    if (!SYS::Helper::is_valid_fd(fd)) {
        return -1;
    }

    return PCB::current().user_files[fd].read(len, buffer);
}

ssize_t SYS::Call::write(int fd, void* buffer, size_t len) {
    if (!SYS::Helper::is_valid_fd(fd)) {
        return -1;
    }

    return PCB::current().user_files[fd].write(len, buffer);
}

int SYS::Call::pipe(int* write_fd, int* read_fd) {
    int wfd = SYS::Helper::get_next_fd();
    int rfd = SYS::Helper::get_next_fd(wfd);

    if (wfd == -1 || rfd == -1) {
        return -1;
    }

    Shared<UserFileContainer> pipe = Shared<UserFileContainer>::make(new PipeFile());

    PCB::current().user_files[wfd] = OpenFile(pipe, Flags::USER_FILE_WRITE);
    PCB::current().user_files[rfd] = OpenFile(pipe, Flags::USER_FILE_READ);

    *write_fd = wfd;
    *read_fd = rfd;

    return 0;
}

int SYS::Call::dup(int fd) {
    if (!SYS::Helper::is_valid_fd(fd)) {
        return -1;
    }

    // search for a new place
    int new_fd = SYS::Helper::get_next_fd();
    if (new_fd == -1) {
        return -1;
    }

    // copy the fd
    PCB::current().user_files[new_fd] = PCB::current().user_files[fd];

    return new_fd;
}

// =================================================================
// ============================ HELPER =============================
// =================================================================

void* SYS::Helper::setup_initial_user_stack(void* user_esp, char** args) {
    /**
     * Structure of the stack (for main) to build:
     *
     *              return_addr     provided by user function _start
     *  esp ->      argc
     *              argv            (pointer to arg array (arg_arr))
     *              char[]          arg0 char[]
     *              char[]          arg1 char[]
     *              ...
     *              char[]          arg(argc-1) char[]
     *  arg_arr ->  argv[0]         (pointer to arg0 char*)
     *              argv[1]         (pointer to arg1 char*)
     *              ...
     *              argv[argc-1]    (pointer to arg(argc-1) char*)
     *  bot ->      0               (null terminated array)
     */

    uint32_t* bot = ((uint32_t*)user_esp) - 1;
    bot[0] = 0;

    // get argc (1 more because of program path)
    uint32_t argc = 0;
    while (args[argc]) {
        argc++;
    }

    // create arg buffers
    char** arg_arr = ((char**)bot) - argc;
    char* arg = (char*)arg_arr;

    // fill stack with args
    for (uint32_t i = 0; i < argc; i++) {
        arg -= K::strlen(args[i]) + 1;
        K::strcpy(arg, args[i]);
        arg_arr[i] = arg;
    }

    // add argc, argv
    constexpr uint32_t alignment = 16;
    uint32_t aligned_top = ((((uint32_t)arg) / alignment) * alignment);
    uint32_t* esp = ((uint32_t*)aligned_top) - 2;
    esp[1] = (uint32_t)arg_arr;
    esp[0] = argc;

    // we are done!
    return esp;
}

bool SYS::Helper::try_user_exception_handler(int type, unsigned arg) {
    using namespace SmartVMM::Helper;

    // dont allow reentrant exception handling
    if (PCB::current().in_signal_handler || PCB::current().handler == nullptr) {
        return false;
    }

    uint32_t* user_esp = (uint32_t*)PCB::current().regs.esp_user;
    uint32_t* user_esp_handler = user_esp - 3;

    // dont allow our selves to overflow the user stack
    uint32_t is_user_safe = 0;
    uint32_t pages_to_check = page_num((uint32_t)user_esp) - page_num((uint32_t)user_esp_handler) + 1;
    foreach_allocated_vpn(PCB::current().mmap_tree,
                          page_num((uint32_t)user_esp), pages_to_check,
                          Flags::MMAP_REAL | Flags::MMAP_RW | Flags::MMAP_USER, 0,
                          [&is_user_safe](PageNum pn, MMAPBlock* block) { is_user_safe++; return 0; });
    if (is_user_safe < pages_to_check) {
        return false;
    }

    // at this point we are safe so lets do it
    PCB::current().push_state();
    user_esp_handler[0] = VA_IMPLICIT_SIGRET;
    user_esp_handler[1] = type;
    user_esp_handler[2] = arg;
    PCB::current().in_signal_handler = true;
    PCB::current().regs.eip = (uint32_t)PCB::current().handler;
    PCB::current().regs.esp_user = (uint32_t)user_esp_handler;
    resume_or_yield();
    return false;
}

int SYS::Helper::get_next_fd(int after) {
    // search for a new place
    for (int fd = K::max(0, after) + 1; fd < 10; fd++) {
        if (!is_valid_fd(fd)) {
            return fd;
        }
    }
    return -1;
}

bool SYS::Helper::is_valid_fd(int fd) {
    return fd >= 0 && fd < 10 && PCB::current().user_files[fd].uf != Shared<UserFileContainer>::NUL;
}
