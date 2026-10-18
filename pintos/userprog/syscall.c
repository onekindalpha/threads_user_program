#include "userprog/syscall.h"
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
void syscall_handler (struct intr_frame *);
static void check_address (const void *addr);
void sys_exit (int status);
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
struct lock filesys_lock;

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
	if (addr == NULL)
		sys_exit(-1);
	/* 커널 주소이면 */
	if (!is_user_vaddr(addr))
		sys_exit(-1);
	/* 매핑 되지 않았으면 */
	if (pml4_get_page(thread_current()->pml4, addr) == NULL)
		sys_exit(-1);
}


//power_off가 선언된 헤더(Threads/init.h)를 syscall.c에서 include하고 있음. 
void
syscall_handler (struct intr_frame *f) {
	switch (f->R.rax) { 
		case SYS_WRITE:
		  // rdi:fd, rsi:buffer, rdx:size
			/* fd가 1이면 putbuf(buffer, size), 반환값은 f->R.rax에*/
			if (f->R.rdi == 1)
				putbuf((const char *) f->R.rsi, (size_t) f->R.rdx);
				f->R.rax = f->R.rdx;
			break;
		// exit관련 함수가. 
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
			check_address(file);	 
			/* 락 잡기 */
			lock_acquire(&filesys_lock);
			/* filesys_create(이름, 크기 결과를 )*/
			f-> R.rax = filesys_create(file, initial_size);
			/* 락 풀기 */
			lock_release(&filesys_lock);
			break;
		}
		// case SYS_OPEN: {
		// 	/* rdi: 파일 이름 주소 */
		// 	const char *file = (const char *) f->R.rdi;
		// }
		case SYS_REMOVE: {
			const char *file = (const char *) f->R.rdi;
			check_address(file);
			lock_acquire(&filesys_lock);
			f->R.rax = filesys_remove(file);
			lock_release(&filesys_lock);
			break;
		}
		case SYS_HALT: {
			power_off();
			break;
		}
		 default:
			thread_exit ();
 }
}

