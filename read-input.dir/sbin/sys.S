	#
	# user-side system calls
	#
	# System calls use a special convention:
        #     %eax  -  system call number
        #
        #

	# void exit(int status)
	.global exit
exit:
	mov $0,%eax
	int $48
	ret

	# ssize_t write(int fd, void* buf, size_t nbyte)
	.global write
write:
	mov $1,%eax
	int $48
	ret

        # int fork()
        .global fork
fork:
        push %ebx
        push %esi
        push %edi
        push %ebp
        mov $2,%eax
        int $48
        pop %ebp
        pop %edi
        pop %esi
        pop %ebx
        ret

	# int shutdown(void)
        .global shutdown
shutdown:
        mov $7,%eax
        int $48
        ret

	# int execl(const char *pathname, const char *arg, ...
        #               /* (char  *) NULL */);
        .global execl
execl:
	mov $1000,%eax
	int $48
	ret


        # unsigned sem()
        .global sem
sem:
	mov $1001,%eax
	int $48
	ret

        # void up(unsigned)
        .global up
up:
	mov $1002,%eax
	int $48
	ret

        # void down(unsigned)
        .global down
down:
	mov $1003,%eax
	int $48
	ret

	# void simple_signal(handler)
	.global simple_signal
simple_signal:
	mov $1004,%eax
	int $48
	ret

	# void simple_mmap(void*, unsigned)
	.global simple_mmap
simple_mmap:
	mov $1005,%eax
	int $48
	ret

	# int sigreturn(void)
	.global sigreturn
sigreturn:
	mov $1006,%eax
	int $48
	ret

	# int sem_close(int)
	.global sem_close
sem_close:
	mov $1007,%eax
	int $48
	ret
	
	# int simple_munmap(void* addr)
	.global simple_munmap
simple_munmap: 
	mov $1008, %eax
	int $48
	ret
        # int join()
        .global join
join:
	mov $999,%eax
	int $48
	ret

	# void chdir(char* path)
	.global chdir
chdir:
	mov $1020,%eax
	int $48
	ret

	# int open(char* path)
	.global open
open:
	mov $1021,%eax
	int $48
	ret

	# int tui()
	.global tui
tui:
	mov $1101,%eax
	int $48
	ret

	# int set_tui(int fd)
	.global set_tui
set_tui:
	mov $1102,%eax
	int $48
	ret

	# int close(int fd)
	.global close
close:
	mov $1022,%eax
	int $48
	ret

	# int len(int fd)
	.global len
len:
	mov $1023,%eax
	int $48
	ret

	# int read(int fd, void* buffer, unsigned count)
	.global read
read:
	mov $1024,%eax
	int $48
	ret

	# int pipe(int* write_fd, int* read_fd)
	.global pipe
pipe:
	mov $1026,%eax
	int $48
	ret

	# int dup(int fd)
	.global dup
dup:
	mov $1028,%eax
	int $48
	ret

	# char getch()
	.global getch
getch:
	mov $1100,%eax
	int $48
	ret