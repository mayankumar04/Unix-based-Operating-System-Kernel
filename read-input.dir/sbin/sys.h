#ifndef _SYS_H_
#define _SYS_H_

/****************/
/* System calls */
/****************/

typedef int ssize_t;
typedef unsigned int size_t;

/* exit */
extern void exit(int rc);

/* write */
extern ssize_t write(int fd, void* buf, size_t nbyte);

/* fork */
extern int fork();

/* execl */
extern int execl(const char *pathname, const char *arg, ...
                       /* (char  *) NULL */);

/* shutdown */
extern void shutdown(void);

/* join */
extern int join(void);

/* sem */
extern int sem(unsigned int);

/* up */
extern int up(unsigned int);

/* down */
extern int down(unsigned int);

/* sem_close */
extern int sem_close(int s);

//1005
extern void* simple_mmap(void* addr, unsigned size, int fd, unsigned offset);

/* simple_signal */
extern void simple_signal(void (*pf)(int, unsigned int));

extern void sigreturn(); 

//1008
extern int simple_munmap(void* addr); 

//1020
extern void chdir(char* path);

//1021
extern int open(char* path);

//1022
extern int close(int fd);

//1023
extern int len(int fd);

//1024
extern int read(int fd, void* buffer, unsigned count);

//1026
extern int pipe(int* write_fd, int* read_fd);

//1027
extern int dup(int fd);

// 1100
extern char getch();

//1101
extern int tui();

//1102
extern int set_tui(int fd);

#endif