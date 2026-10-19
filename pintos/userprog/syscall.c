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

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) 
{
	/// @brief 시스템콜 작동 모아놓은 함수 
	/// @warning 유저프로그램에서 NULL이나 이상한 주소를 가리킬 수 있기 때문에 향후 안전검사 추가 필요함 만약 안전검사 통과 못할시 해당 프로세스 kill하는 절차가 필요함
	switch (f->R.rax)
	{
	case SYS_WRITE: /** @brief 문자열 출력 */
					/// @details 유저프로그램 write로부터 fd, buffer 주소, size 전달받은 후 실제 문자열 반환한다. 그리고 exit
		{
			if (f->R.rdi == 1)
				putbuf((const char *)f->R.rsi, f->R.rdx);
			f->R.rax = f->R.rdx;
			break;
		}

	case SYS_EXIT: /** @brief 함수종료*/
				   /// @details 스레드 네임에서 종료될 스레드의 이름을 변수에 저장후 화면에 출력 status는 userprogram에서 전달함
		{
			const char *name = thread_name();
			int status = f->R.rdi;
			printf("%s: exit(%d)\n", name, status);
			thread_exit();
			break;
		}
	}
}
