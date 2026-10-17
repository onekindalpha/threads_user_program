#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

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
}

//power_off가 선언된 헤더(Threads/init.h)를 syscall.c에서 include하고 있음. 
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
			// status를 int로 꺼내기. %s: exit(%d)\n 형식으로 출력. 스레드 종료
			int a = (int) f->R.rdi;
			printf("%s: exit(%d)\n", thread_current()->name, a);
			thread_exit();
			break;
		 }
		 default:
			thread_exit ();
 }
}