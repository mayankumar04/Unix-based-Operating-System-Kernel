#ifndef _SYS_H_
#define _SYS_H_

#include "process.h"
#include "stdint.h"

using namespace ProcessManagement;

class SYS {
   public:
    // init
    static void init(void);

    // sys calls - premis here is that register state is the last arg if its needed
    struct Call {
        enum EAX {
            EXIT = 0,
            WRITE1 = 1,
            FORK = 2,
            SHUTDOWN = 7,
            EXECL1 = 9,
            DUP2 = 41, //0x29 on sheet
            GETCWD = 183, //0xb7 on sheet
            YIELD = 998,
            JOIN = 999,
            EXECL2 = 1000,
            SEM = 1001,
            UP = 1002,
            DOWN = 1003,
            SIMPLE_SIGNAL = 1004,
            SIMPLE_MMAP = 1005,
            SIGRETURN = 1006,
            SEM_CLOSE = 1007,
            SIMPLE_MUNMAP = 1008,
            CHDIR = 1020,
            OPEN = 1021,
            CLOSE = 1022,
            LEN = 1023,
            READ = 1024,
            WRITE2 = 1025,
            PIPE = 1026,
            DUP = 1028,
            GETCH = 1100,
            TUI  = 1101,
            SET_TUI = 1102
        };

        // handles the syscall
        static int handle_syscall(EAX eax, RegisterState* regs);

        // sycalls
        static void exit(int rc);                                                      // 0
        static int fork();                                                             // 2
        static void shutdown();                                                        // 7
        static void yield();                                                           // 998
        static void join();                                                            // 999
        static int execl(char* program_path, char** args);                             // 9 or 1000
        static int getcwd(char* buf, unsigned size);                                   // 183
        static int sem(unsigned n);                                                    // 1001
        static int up(unsigned sem_desc);                                              // 1002
        static int down(unsigned sem_desc);                                            // 1003
        static void simple_signal(void (*handler)(int, unsigned));                     // 1004
        static void* simple_mmap(void* addr, unsigned size, int fd, unsigned offset);  // 1005
        static int sigreturn();                                                        // 1006
        static int sem_close(int sem_desc);                                            // 1007
        static int simple_munmap(void* addr);                                          // 1008
        static void chdir(char* path);                                                 // 1020
        static int open(const char* path);                                             // 1021
        static int close(int fd);                                                      // 1022
        static int len(int fd);                                                        // 1023
        static ssize_t read(int fd, void* buffer, size_t len);                         // 1024
        static ssize_t write(int fd, void* buffer, size_t len);                        // 1 or 1025
        static int pipe(int* write_fd, int* read_fd);                                  // 1026
        static int dup(int fd);
        static char getch();
        static int tui();
        static bool set_tui(int tui_id);
    };

    struct Helper {
        // sets up stack to pass arguments to main
        // assumes that all data is in kernel space
        static void* setup_initial_user_stack(void* user_esp, char** args);

        static constexpr VirtualAddress VA_IMPLICIT_SIGRET = 3;

        /**
         * tries to allow the current user process to handle the exception
         * assumes the user state is already saved in the current pcb
         */
        static bool try_user_exception_handler(int type, unsigned arg);

        /**
         * searches for a free file descriptor that is > than the given one
         * for the current process
         */
        static int get_next_fd(int after = -1);

        /**
         * checks if the given fd is valid for the current process
         */
        static bool is_valid_fd(int fd);

        // NOTE : maybe template the valid desc cuz we use this for other descs ??
    };
};

#endif
