#ifndef _SYS_H_
#define _SYS_H_

/****************/
/* System calls */
/****************/

typedef int ssize_t;
typedef unsigned int size_t;

/* exit */
extern void exit(int rc);

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

/* yield */
extern void yield(void);

/* simple_signal */
extern void simple_signal(void (*pf)(int, unsigned int));

/* simple_mmap */
extern unsigned int simple_mmap(unsigned int addr, unsigned int size, int fd, unsigned int offset);

/* simple_munmap */
extern int simple_munmap(unsigned int addr);

/* sigreturn */
extern int sigreturn(void);

/* chdir */
void chdir(char* path);

/* open */
int open(char* path);

/* close */
int close(int fd);

/* len */
int len(int fd);

/* read */
int read(int fd, void* buffer, unsigned count);

/* write */
int write(int fd, void* buffer, unsigned count);

/* pipe */
int pipe(int* write_fd, int* read_fd);

#endif
