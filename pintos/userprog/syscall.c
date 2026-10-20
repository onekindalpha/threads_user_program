#include "userprog/syscall.h"
#include "devices/input.h"
#include <string.h>              
#include "threads/palloc.h"      
#include "userprog/process.h"    
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
void syscall_entry (void);
static void check_string (const char *str);
void syscall_handler (struct intr_frame *);
static void check_address (const void *addr);
void sys_exit (int status);
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
struct lock filesys_lock;
#include "filesys/file.h"
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */
#define FD_TABLE_SIZE ((int) (PGSIZE / sizeof (struct file *)))

#define FD_MAX 512
void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	// syscall_init에서 부팅시 초기화
	lock_init(&filesys_lock);
}

/* int fd를 반환하는 함수 */
static int
fd_add(struct file *file) {
	struct thread *cur = thread_current();
	for (int fd = 2; fd < FD_TABLE_SIZE; fd++) {
		if (cur->fd_table[fd] == NULL) {
				cur->fd_table[fd] = file;
				return fd;
		}
	}
	return -1;
}

/* 파일 포인터 반환하는 함수 */
static struct file *
fd_get(int fd) {
	struct thread *cur = thread_current();
	if (fd < 2 || fd >= FD_TABLE_SIZE || 
		cur->fd_table[fd] == NULL) {
		return NULL;
	}
	return cur->fd_table[fd];
}

/* 아무것도 반환하지 않는 함수 */
static void
fd_remove(int fd) {
	struct thread *cur = thread_current();
	if (fd < 2 || fd >= FD_TABLE_SIZE)
		return;
	cur->fd_table[fd] = NULL;
}

/* 종료 메시지 출력 및 thread_exit() 을 부름. */
void
sys_exit (int status) {
	if (lock_held_by_current_thread(&filesys_lock))
		lock_release(&filesys_lock);
	thread_current()->exit_status = status;
	printf("%s: exit(%d)\n", thread_current()->name, status);
	thread_exit();
}

static void
check_address(const void *addr) {
	/* 유저 포인터가 널이면*/
	if (addr == NULL) {
		sys_exit(-1);
	}
	/* 커널 주소이면 */
	if (!is_user_vaddr(addr)) {
		sys_exit(-1);
	 }
	/* 매핑 되지 않았으면 */
	if (pml4_get_page(thread_current()->pml4, addr) == NULL) {
		sys_exit(-1);
		}
}

static void
check_string(const char *str) {
	while (true) {
		check_address(str);
		if (*str == '\0')
			return;
		str++;
	}
}

static void
check_buffer (const void *buf, unsigned size) {		
	/* 요청 사이즈 */
	if (size == 0) 
			return;
	/* 버퍼의 실제 시작 주소 */
	uintptr_t start = (uintptr_t) buf;
	/* 주소 덧셈의 오버플로 검사 */
	if (start > UINTPTR_MAX - (size - 1))
		sys_exit(-1);
	uintptr_t end = start + size -1 ;
	/* 버퍼의 시작주소와 마지막 주소 검사*/
	check_address(buf);
	check_address((const void *)end);
	/* 검사할 첫 페이지의 시작 주소 계산 */
	uintptr_t first_page = (uintptr_t) pg_round_down((void *) start);
	/* 검사할 마지막 페이지의 시작 주소 계산 */
	uintptr_t last_page = (uintptr_t) pg_round_down((void *) end);
	/* 버퍼가 걸치는 모든 페이지 검사 */
	for (uintptr_t page = first_page; page <=last_page ; page += PGSIZE) {
		check_address((void *) page);
	}
}

//power_off가 선언된 헤더(Threads/init.h)를 syscall.c에서 include하고 있음. 
void
syscall_handler (struct intr_frame *f) {
	switch (f->R.rax) { 
		case SYS_WRITE: {
		/* rdi: fd, rsi: buffer, rdx: size */
		int fd = (int) f->R.rdi;
		const void *buffer = (const void *) f->R.rsi;
		unsigned size = (unsigned) f->R.rdx;
		/* write-zero: 0바이트 쓰기는 0 반환 */
		if (size == 0) {
			f->R.rax = 0;
			break;
		}
		/* write-bad-ptr: 버퍼가 유효하지 않으면 여기서 exit(-1) */
		check_buffer(buffer, size);
		/* stdout */
		if (fd == 1) {
			putbuf((const char *) buffer, (size_t) size);
			f->R.rax = size;
			break;
		}
		/* stdin(0)이나 음수 fd에는 쓸 수 없음 */
		if (fd < 2) {
			f->R.rax = -1;
			break;
		}
		/* 일반 파일 */
		struct file *file = fd_get(fd);
		if (file == NULL) {          /* write-bad-fd */
			f->R.rax = -1;
			break;
		}
		lock_acquire(&filesys_lock);
		f->R.rax = file_write(file, buffer, (off_t) size);
		lock_release(&filesys_lock);
		break;
	}
		// exit관련 함수가. 
		case SYS_FILESIZE:
		{
			int fd = (int)f->R.rdi;
			struct file *file = fd_get(fd);
			/** @brief fd번호에 열린파일이 없을때 return값 -1 */
			if (file == NULL)
			{
				f->R.rax = -1;
				break;
			}
			lock_acquire(&filesys_lock);
			off_t size = file_length(file);/** @brief file_length 내장함수를 활용해 전체 크기인 size값 반환 */
			lock_release(&filesys_lock);
			f->R.rax = size;
			break;
		}
		case SYS_TELL:
		{
			int fd = (int)f->R.rdi;
			struct file *file = fd_get(fd);
			if (file == NULL)
			{
				f->R.rax = -1;
				break;
			}
			lock_acquire(&filesys_lock);
			off_t cur_pos = file_tell(file);/** @brief file_tell 내장함수를 활용해 현재위치값을 off-set으로 반환 */
			lock_release(&filesys_lock);
			f->R.rax = cur_pos;
			break;
		}
		case SYS_SEEK:
		{
			int fd = (int)f->R.rdi;
			unsigned position = (unsigned)f->R.rsi;
			struct file *file = fd_get(fd);
			if (file == NULL)
			{
				f->R.rax = -1;
				break;
			}
			lock_acquire(&filesys_lock);
			file_seek(file,(off_t)position);/** @brief file_seek 내장함수를 활용해 file구조체에 저장된 pos를 position으로 옮김 */
			lock_release(&filesys_lock);
			f->R.rax = position;
			break;
		}
		case SYS_EXIT: {
			sys_exit ((int) f->R.rdi);
			break;
		 }
		// create
		case SYS_CREATE: {
			/*rdi: 파일 이름 주소, rsi: 초기 크기 */
			const char *file = (const char *) f->R.rdi;
			unsigned initial_size = (unsigned) f->R.rsi;
			/* 유저 포인터 검증 함수 */
			check_string(file);	 
			/* 락 잡기 */
			lock_acquire(&filesys_lock);
			/* filesys_create(이름, 크기 결과를 )*/
			f-> R.rax = filesys_create(file, initial_size);
			/* 락 풀기 */
			lock_release(&filesys_lock);
			break;
		}
		case SYS_OPEN: {
			const char *name = (const char *) f->R.rdi;
			check_string(name);
			lock_acquire(&filesys_lock);
			struct file *file = filesys_open(name);
			
			if (file == NULL) {
				f->R.rax = -1;
			} else {
				int fd = fd_add(file);
				if (fd == -1) {
					file_close(file);
					f->R.rax = -1;
					} else {
					f->R.rax = fd;
				}
		}
		lock_release(&filesys_lock);
		break;
		}
		case SYS_READ: {
			int fd = (int) f->R.rdi;
			/* 이게 전달을 받은 버퍼의 주소임 */
			void *buffer = (void *) f->R.rsi;
			unsigned size = (unsigned) f->R.rdx;
			// printf("[SYS_READ] fd=%d, buffer=%p, size=%u\n",
			// 			fd, buffer, size);
			/* 요청크기가 0이면 */
			if (size == 0) {
				f->R.rax = 0;
				break;
			}
			/* fd가 1인경우 -1을 반환 */
			if (fd < 0 || fd== 1) {
				/* 아마 새기고 반환을 하는 것임. */
				f->R.rax = -1;
				break;
			}

			// printf("[SYS_READ] before check_buffer\n");
    	check_buffer(buffer, size);
    	// printf("[SYS_READ] after check_buffer\n");
			/* 일반 파일이면 */
			if (fd >=2) {
				struct file *file = fd_get(fd);
				// printf("[SYS_READ] file=%p\n", file);
				if (file == NULL)
					sys_exit(-1);
				lock_acquire(&filesys_lock);
				f->R.rax = file_read(file, buffer, size);
				lock_release(&filesys_lock);
				// printf("[SYS_READ] file_read returned=%lld\n",
				// 		(long long) f->R.rax);
			}
			/* fd == 0일때 키보드 입력을 받음 */
			if (fd == 0) {
			unsigned i;
			for (i = 0; i < size; i++) {	
				((uint8_t *) buffer)[i] = input_getc();
				}
				f->R.rax= i;
				break;
			}
			break;
		}
		case SYS_CLOSE: {
			int fd = (int) f->R.rdi;
			/* fd 테이블 조회 후 */
			struct file *file = fd_get(fd);
			if (file != NULL) {
				lock_acquire(&filesys_lock);
				file_close(file);
				fd_remove(fd);
				lock_release(&filesys_lock);
			}
		break;
		}
		case SYS_REMOVE: {
			const char *name = (const char *) f->R.rdi;
			check_string(name);
			lock_acquire(&filesys_lock);
			f->R.rax = filesys_remove(name);
			lock_release(&filesys_lock);
			break;
		}
		case SYS_HALT: {
			power_off();
			break;
		}
		case SYS_EXEC:
		{
			const char *cmd_line = (const char *)f->R.rdi;
			check_string(cmd_line);
			void *exec_pagetable = palloc_get_page(0);
			size_t exec_len = strlen(cmd_line);
			strlcpy(exec_pagetable, cmd_line, PGSIZE);
			process_exec(exec_pagetable);
			sys_exit(-1);
			break;
		}
		 default:
			thread_exit ();
 }
}
