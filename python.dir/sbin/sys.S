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
	int $0x80
	ret
	
	# int fork()
	.global fork
fork:
	push %ebx
	push %esi
	push %edi
	push %ebp
	mov $2,%eax
	int $0x80
	pop %ebp
	pop %edi
	pop %esi
	pop %ebx
	ret

	# int shutdown(void)
	.global shutdown
shutdown:
	mov $7,%eax
	int $0x80
	ret

	# int execl(const char *pathname, const char *arg, ...
	#               /* (char  *) NULL */);
	.global execl
execl:
	mov $1000,%eax
	int $0x80
	ret


	# unsigned sem()
	.global sem
sem:
	mov $1001,%eax
	int $0x80
	ret

	# void up(unsigned)
	.global up
up:
	mov $1002,%eax
	int $0x80
	ret

	# void down(unsigned)
	.global down
down:
	mov $1003,%eax
	int $0x80
	ret

	# void simple_signal(handler)
	.global simple_signal
simple_signal:
	mov $1004,%eax
	int $0x80
	ret

	# void simple_mmap(void*, unsigned)
	.global simple_mmap
simple_mmap:
	mov $1005,%eax
	int $0x80
	ret

	# void simple_munmap(void*, unsigned)
	.global simple_munmap
simple_munmap:
	mov $1008,%eax
	int $0x80
	ret

	# int sigreturn(void)
	.global sigreturn
sigreturn:
	mov $1006,%eax
	int $0x80
	ret

	# int join()
	.global join
join:
	mov $999,%eax
	int $0x80
	ret

	# int yield()
	.global yield
yield:
	mov $998,%eax
	int $0x80
	ret
	
	# int chdir(char*)
	.global chdir
chdir:
	mov $1020,%eax
	int $0x80
	ret
	
	# int open(char*)
	.global open
open:
	mov $1021,%eax
	int $0x80
	ret
	
	# int close(int)
	.global close
close:
	mov $1022,%eax
	int $0x80
	ret
	
	# int len(int)
	.global len
len:
	mov $1023,%eax
	int $0x80
	ret
	
	# int read(int, void*, unsigned)
	.global read
read:
	mov $1024,%eax
	int $0x80
	ret
	
	# int write(int, void*, unsigned)
	.global write
write:
	mov $1025,%eax
	int $0x80
	ret
	
	# int pipe(int*, int*)
	.global pipe
pipe:
	mov $1026,%eax
	int $0x80
	ret

