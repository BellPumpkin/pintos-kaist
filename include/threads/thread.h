#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
/* thread의 life cycle 상태 */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
/*
스레드 식별자 유형입니다. 원하는 유형으로 다시 정의할 수 있습니다.*/

typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. - tid_t에 대한 오류 값 */

/* Thread priorities. - 스레드 우선 순위 */
#define PRI_MIN 0                       /* Lowest priority. - 낮은 우선 순위 */
#define PRI_DEFAULT 31                  /* Default priority. - 기본 우선 순위 */
#define PRI_MAX 63                      /* Highest priority. - 높은 우선 순위 */

/* A kernel thread or user process.
 * 커널 스레드 또는 사용자 프로세스
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 * 각 스레드 구조는 자신의 4kB 페이지에 저장됩니다. 스레드 구조 자체는 페이지의 맨 아래에 위치합니다(오프셋 0). 
 * 나머지 페이지는 페이지 상단에서 아래쪽으로 증가하는 스레드의 커널 스택을 위해 예약됩니다(오프셋 4kB).
 * 다음은 예시입니다:
 * 
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold: // 이것의 결과는 두배 입니다. or 이것의 결론은 두 가지이다.
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    1. 첫째, '구조 스레드'는 너무 커져서는 안 됩니다. 
 * 		 그러면 커널 스택을 위한 공간이 충분하지 않게 됩니다. 
 * 		 우리의 베이스 '구조 스레드'는 크기가 몇 바이트밖에 되지 않습니다. 
 * 		 아마 1kB 미만으로 유지될 것입니다.
 *  
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 * 
 * 	  2. 커널 스택이 너무 커지지 않도록 해야 합니다. 
 * 		 스택이 오버플로되면 스레드 상태가 손상됩니다.
 * 		 따라서 커널 함수는 큰 구조나 배열을 비정규 로컬 변수로 할당하지 않아야 합니다.
 * 		 대신 malloc() 또는 palloc_get_page()로 동적 할당을 사용하십시오.
 * 
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion.
 * 
 * 이러한 문제 중 하나의 첫 번째 증상은 아마도 실행 중인 스레드의 '구조 스레드'의 '매직' 멤버가 TRADE_MAGIC으로 설정되어 있는지 확인하는 thread_current()에서 어설션 실패일 것입니다.
 * 스택 오버플로는 일반적으로 이 값을 변경하여 어설션을 트리거합니다.
 */

/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list.
 * 
 * 'elem' 멤버는 이중 목적을 가지고 있습니다.
 * 실행 큐의 요소가 될 수도 있고(thread.c), 세마포어 대기 목록의 요소가 될 수도 있습니다(synch.c).
 * 서로 배타적이기 때문에 이 두 가지 방식으로만 사용할 수 있습니다.
 * 준비 상태의 스레드만 실행 큐에 있고 차단 상태의 스레드만 세마포어 대기 목록에 있습니다.
 */


struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. - 스레드 식별자 */
	enum thread_status status;          /* Thread state. - 스레드 상태 */
	char name[16];                      /* Name (for debugging purposes). - 이름 (디버깅 목적으로) */
	int priority;                       /* Priority. - 우선순위 1~63 */
	int64_t wake_tick;					// 일어나야 할 시간

	/* Shared between thread.c and synch.c. - thread.c와 synch.c 간에 공유됩니다. */
	struct list_elem elem;              /* List element. - 리스트 요소 */

	// 수정!!!!
	int init_priority;		// 수정!!!, 양도 받았다가 원래의 값으로 돌아가기 위한 값
	struct lock *wait_on_lock;	// 스레드가 현재 얻기 위해 기다리는 lock
	struct list donations;		// 현재 thread에게 priority를 나눠준 쓰레드 리스트
	struct list_elem donation_elem; // donations 리스트를 관리하기 위한 elem
	

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. - 스레드가 소유 */
	struct intr_frame tf;               /* Information for switching - switching을 위한 정보 */
	unsigned magic;                     /* Detects stack overflow. - 스택 오버플로를 탐지 */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs".

   false(기본값)인 경우 라운드 로빈 스케줄러를 사용합니다.
   참일 경우 다단계 피드백 큐 스케줄러를 사용합니다.
   커널 명령줄 옵션 "-olfqs"에 의해 제어됩니다. */

extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

bool cmp(struct list_elem *state_a, struct list_elem *state_b, void *aux UNUSED);
bool cmp_thread_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void preempt_priority(void);
void donate_priority(void);
void update_priority_before_donations(void);

bool cmp_donation_priority(const struct list_elem *a, const struct list_elem *b, void *aux);
void donate_priority(void);
void remove_donor(struct lock *lock);
void update_priority_before_donations(void);


bool cmp_sema_priority(const struct list_elem *a, const struct list_elem *b, void *aux);

#endif /* threads/thread.h */
