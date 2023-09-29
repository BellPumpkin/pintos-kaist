#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details.
   
   struct thread's의 magic 멤버에 대한 임의의 값, 스택 오버플로를 감지하는 데 사용됩니다.
   자세한 내용은 thread.h 상단의 코멘트를 참조하십시오. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value.

   기본 스레드에 대한 임의 값 이 값을 수정하지 마십시오. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running.

   TRADE_READY 상태의 프로세스,
   즉 실행 준비가 되었지만 실제로 실행되지 않는 프로세스의 목록입니다. */
static struct list ready_list;
static struct list sleep_list; // 재울 애들을 저장

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). - allocate_tid()에서 사용한 Lock */
static struct lock tid_lock;

/* Thread destruction requests - 스레드 삭제 요청 */
// 쓰레드 파괴 요청들이 담기는 list
// 한번에 하나밖에 못함 -> 파괴도 하나씩?
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);
bool cmp(struct list_elem *state_a, struct list_elem *state_b, void *aux UNUSED);
bool priority_cmp(struct list_elem *state_a, struct list_elem *state_b, void *aux UNUSED);
void thread_test_preemption(void);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread.
 * 
 * 실행 중인 스레드를 반환합니다.
 * CPU의 스택 포인터 rsp를 읽은 다음 해당 값을 페이지의 시작 지점으로 내림(바로 전 페이지 경계로 맞춤)합니다.
 * struct thread 구조체는 항상 페이지의 시작 부분에 있으며,
 * 스택 포인터는 중간에 위치하므로 이렇게 하면 현재 스레드를 찾을 수 있습니다
 * 
 * rsp 레지스터는 현재 스레드가 사용하는 스택의 최상단을 가리킵니다.
 * 스택은 후입선출(LIFO) 구조로 데이터를 저장하고 검색하는 데 사용되며,
 * 함수 호출 및 복귀, 로컬 변수의 할당 및 해제, 중요한 컨텍스트 정보의 저장과 복원과 같은 작업을 위해 필요합니다.
 */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   현재 실행 중인 코드를 스레드로 변환하여 스레드 시스템을 초기화합니다. 
   이것은 일반적으로 작동할 수 없으며 이 경우에만 가능합니다. 
   Loader.S가 스택의 하단을 페이지 경계에 두도록 주의를 기울였습니다.

   Also initializes the run queue and the tid lock.

   또한 실행 queue 및 tid lock을 초기화합니다.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   이 함수를 호출한 후 thread_create()로 스레드를 만들기 전에 page allocator를 초기화해야 합니다.

   It is not safe to call thread_current() until this function
   finishes.
   
   이 기능이 완료될 때까지 thread_current()를 호출하는 것은 안전하지 않습니다. */

void
thread_init (void) {
	// 거짓(False)인 경우, assert는 오류 메시지를 출력하고 프로그램을 중단
	ASSERT (intr_get_level () == INTR_OFF); // (플래그 레지스터의 값이 FLAG_IF값이 0이면) == 인터럽트 사용안함이 True면 패스

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init ().
	 * 
	 * 커널을 위한 시간적인 GDT를 다시 로드합니다. 이 GDT에는 사용자 컨텍스트가 포함되어 있지 않습니다. 
	 * 커널은 사용자 컨텍스트를 포함한 GDT를 gdt_init()에서 다시 구축할 것입니다.
	 * */

	// GDT(Global Descriptor Table) 또는 IDT(Interrupt Descriptor Table)와 같은 시스템 디스크립터 테이블을 가리키기 위해 사용됩니다.
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,	// size 필드에는 GDT의 크기를 저장
		.address = (uint64_t) gdt	// address 필드에는 GDT가 메모리에서 시작하는 주소를 저장합니다.
									// (uint64_t) 캐스팅을 사용하여 gdt 배열이 시작하는 메모리 주소를 64비트 정수로 변환하여 저장
	};
	// 이렇게 초기화된 gdt_ds 구조체는 GDT의 크기와 시작 주소를 포함하고 있으며,
	// 이 정보는 시스템 디스크립터 테이블을 설정하거나 업데이트하는 데 사용될 수 있습니다.

	lgdt (&gdt_ds);
	// 이 함수는 lgdt 어셈블리 명령어를 사용하여 GDT 레지스터를 설정합니다.
	// GDT는 x86 아키텍처에서 세그먼트 디스크립터를 저장하는 테이블이며, 이 함수를 통해 GDT를 업데이트할 수 있습니다.
	// GDT를 변경하면 메모리 보호 및 세그먼트 설정과 같은 시스템 레벨의 설정을 조절할 수 있습니다.

	/* Init the globla thread context - 글로벌 스레드 컨텍스트를 초기화하십시오 */
	lock_init (&tid_lock);
	list_init (&ready_list);		// 실행 준비가 되었지만 실제로 실행되지 않는 프로세스의 목록을 초기화
	list_init (&sleep_list);
	list_init (&destruction_req);	// 스레드 삭제 요청하는 목록을 초기화

	

	/* Set up a thread structure for the running thread. - 실행 중인 스레드에 대한 스레드 구조 설정 */
	initial_thread = running_thread ();		// 실행 중인 스레드를 반환
	init_thread (initial_thread, "main", PRI_DEFAULT);	// "main"이라는 이름과 priority가 31인 Thread 구조체 t를 초기화 
	initial_thread->status = THREAD_RUNNING;	// 실행 중
	initial_thread->tid = allocate_tid ();	// 새로운 스레드에 사용할 스레드 ID(tid)를 반환해서 initial_thread->tid 값에 저장
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread.

   인터럽트를 활성화하여 선점형 스레드 스케줄링을 시작합니다.
   또한 아이들 스레드를 생성합니다.
*/
void
thread_start (void) {
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started); // idle thread 생성

	/* Start preemptive thread scheduling. - 선점형 스레드 스케줄링을 시작합니다. */
	intr_enable ();	// 인터럽트 활성화

	/* Wait for the idle thread to initialize idle_thread. - idle_thread가 초기화될 때까지 idle 스레드를 대기합니다." */
	sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3.
   
   주어진 초기 우선 순위(PRIORITY)로 이름이 NAME인 새로운 커널 스레드를 생성하며,
   이 스레드는 AUX를 인자로하여 FUNCTION을 실행한 후 준비 큐(ready queue)에 추가합니다.
   새로운 스레드의 스레드 식별자(TID)를 반환하며, 생성에 실패하면 TID_ERROR를 반환합니다.

   만약 thread_start()가 호출되었다면, 새로운 스레드는 thread_create()가 반환되기 전에 스케줄될 수 있습니다.
   심지어 thread_create()가 반환되기 전에 종료될 수도 있습니다.
   반대로, 원래 스레드는 새로운 스레드가 스케줄되기 전에 어떤 시간이든 실행될 수 있습니다.
   순서를 보장해야 하는 경우 세마포어 또는 다른 형태의 동기화를 사용하십시오.

   제공된 코드는 새로운 스레드의 'priority' 멤버를 PRIORITY로 설정하지만 실제 우선 순위 스케줄링은 구현되지 않았습니다.
   우선 순위 스케줄링은 1-3 문제의 목표입니다.
*/

// 스레드 할당하고, 초기화 해서 실행 대기열에 추가한다~?
// return tid, 스레드 식별자 유형
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. - 스레드 할당 */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. - 스레드 초기화 */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* Call the kernel_thread if it scheduled.
	   Note) rdi is 1st argument, and rsi is 2nd argument.

	   예약된 경우 kernel_thread를 호출합니다.
	   참고) rdi는 첫 번째 인수이고 rsi는 두 번째 인수입니다.*/
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. - 실행 대기열에 추가 */
	thread_unblock (t);

	// ready_list 추가 이후에, ready_list 맨 앞에 있는 쓰레드랑, 현재 러닝중인 쓰레드 비교
	thread_test_preemption();

	return tid;
}

// ready_list 비어 있는 지 확인, thread_current()->priority, list_entry(list_front(&ready_list), thread, elem); 
// yield ()	// 수정!!
void thread_test_preemption(void)
{
    if (!list_empty(&ready_list) && thread_current()->priority 
        < list_entry(list_front(&ready_list), struct thread, elem)->priority)
        thread_yield();
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */

// 현재 돌고 있는 러닝 쓰레드의 상태를 블록으로 바꿔준다. 스케줄링까지 해줌 -> 레디큐의 우선순위 높은 걸 러닝쓰레드로 바꿔준다.
// 인터럽트 플래그는 다양한 상황에서 사용되며, 특히 공유 데이터에 대한 접근 동기화와 스레드 스케줄링에 영향을 미칩니다.
// 인터럽트를 잠시 비활성화하여 크리티컬 섹션에 안전하게 접근하거나, 스레드 간의 상호배제를 구현하는 데 사용
void
thread_block (void) {
	ASSERT (!intr_context ());	// 외부 인터럽트를 처리하고 있지 않으면 True, 
	ASSERT (intr_get_level () == INTR_OFF); // 인터럽트가 OFF 상태면, 
	thread_current ()->status = THREAD_BLOCKED; // 현재 러닝중인 쓰레드 상태를 블록으로 바꿔준다.
	schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */

// block된 쓰레드를 레디 상태로 바꾸고, 레디 큐에 넣어주는 함수
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);		// 
	// list_push_back (&ready_list, &t->elem);		// 해당 쓰레드의 엘레멘트를 넣어준다. -> 상태를 레디 상태로 바꾸고, 
	list_insert_ordered(&ready_list, &t->elem, priority_cmp, NULL); // 수정 내림차순
	t->status = THREAD_READY;		// ready 상태로 만들어 주고
	intr_set_level (old_level);					
}

// 수정!!
bool priority_cmp(struct list_elem *state_a, struct list_elem *state_b, void *aux UNUSED)
{
	    return list_entry (state_a, struct thread, elem)->priority
         > list_entry (state_b, struct thread, elem)->priority;
}


/* Returns the name of the running thread. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */

// 
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow.

	   스레드 T가 실제로 스레드인지 확인하십시오.
	   이러한 어설션 중 하나라도 발생하면 스레드가 스택 오버플로우(stack overflow)에 빠졌을 수 있습니다. 
	   각 스레드는 4 KB 미만의 스택을 가지므로 
	   큰 자동 배열 몇 개나 중간 정도의 재귀 호출은 스택 오버플로우를 발생시킬 수 있습니다.  
	   
	*/
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
// 쓰레드를 없애버린다. (죽인다.)
void
thread_exit (void) {
	// 외부인터럽트가 아닐때,
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();	// 비활성화
	do_schedule (THREAD_DYING);	// 
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */

// 돌고있는 애가 뺏긴다. -> 호출하면 뺏긴다. -> 선점 중인 쓰레드가 레디큐한테 뺏김, 뺏기는 과정은 do_schedule()
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	// 외부 인터럽트 상태가 처리중에는 True를 반환하고, 그외의 모든 시간에는 False를 반환한다. 왜?)  
	// 처리중이 아닐때만 넘어간다.
	ASSERT (!intr_context ());

	// 인터럽트 비활성화 시킴, old_level은 이전 상태를 받아옴, 
	old_level = intr_disable ();
	// 현재 쓰레드가 idle 쓰레드가 아니면 레디 중인 쓰레드가 없다.
	if (curr != idle_thread)
		// list_push_back (&ready_list, &curr->elem); // 레디 큐에 넣는다.
		list_insert_ordered(&ready_list, &curr->elem, priority_cmp, NULL);
	do_schedule (THREAD_READY);		// do_schedule() 현재 작동중인 쓰레드를 죽이지않고, 레디큐에 넣어주기 위해서 (양보당하는 애가 레디상태가 되고, 두 스케줄함수가 현재 러닝중인쓰레드를 인자로 넣어주는 상태로 바꿔주고, 등등) 
	intr_set_level (old_level);
}

void
thread_sleep (int64_t wake_ticks) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	// 외부 인터럽트 상태가 처리중에는 True를 반환하고, 그외의 모든 시간에는 False를 반환한다. 왜?)  
	// 처리중이 아닐때만 넘어간다.
	ASSERT (!intr_context ());

	// 인터럽트 비활성화 시킴, old_level은 이전 상태를 받아옴, 
	old_level = intr_disable ();
	// 현재 쓰레드가 idle 쓰레드가 아니면 레디 중인 쓰레드가 없다.
	if (curr != idle_thread)
		curr->wake_tick = wake_ticks;
		list_insert_ordered (&sleep_list, &curr->elem, cmp, NULL);
		// list_push_back (&ready_list, &curr->elem); // 레디 큐에 넣는다.
	do_schedule (THREAD_BLOCKED);		// do_schedule() 현재 작동중인 쓰레드를 죽이지않고, 레디큐에 넣어주기 위해서 (양보당하는 애가 레디상태가 되고, 두 스케줄함수가 현재 러닝중인쓰레드를 인자로 넣어주는 상태로 바꿔주고, 등등) 
	intr_set_level (old_level);
}

bool cmp(struct list_elem *state_a, struct list_elem *state_b, void *aux UNUSED)
{
	const struct thread *a = list_entry(state_a, struct thread, elem); // 리스트
	const struct thread *b = list_entry(state_b, struct thread, elem);
	return (a->wake_tick < b->wake_tick);
}

void thread_wake(int64_t tick)
{
	// 앞에 뺸
	while(!list_empty(&sleep_list) && list_entry(list_front(&sleep_list),struct thread ,elem)->wake_tick <= tick)
	{
		struct list_elem *front_elem = list_pop_front(&sleep_list);
		thread_unblock(list_entry(front_elem, struct thread, elem));
	}
}



/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) {
	thread_current ()->priority = new_priority; // main_thread->priority가 Default에서 33으로 변경
	thread_test_preemption();
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.
   idle 스레드. 다른 스레드가 실행 준비 상태에 없을 때 실행됩니다

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty.
   
   idle 스레드는 초기에 thread_start()에 의해 준비 목록(ready list)에 추가됩니다. 초기에 한 번 스케줄되며,
   그때 idle_thread를 초기화하고, thread_start()가 계속되도록 전달된 세마포어를 'up'하고 즉시 블록됩니다.
   이후로는 idle 스레드가 준비 목록(ready list)에 나타나지 않습니다.
   준비 목록이 비어 있는 경우, idle 스레드는 next_thread_to_run()에서 특별한 경우로 반환됩니다."
   */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current (); // 현재 실행 중인 쓰레드를 가리키는 포인터 (실제 쓰레드인지 확인)

	// 이 함수는 스레드 간의 대기(wait)와 신호(signal)을 이용하여 스레드의 실행을 동기화하는데 사용되며,
	// 세마포어의 값을 증가시킴으로써 대기 중인 스레드 중 하나를 깨워서 실행 가능한 상태로 만들 수 있습니다.
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. - 다른 누군가에게 실행을 맡깁니다 */
		intr_disable ();	// 인터럽트 비활성화
		thread_block (); 	// 현재 스레드를 block 시킵니다. 
							// 이 작업은 아이들 스레드를 대기 상태로 만들고 다른 스레드가 실행될 수 있도록 합니다.

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory"); // 인터럽트를 활성화 -> CPU를 대기 상태로 전환
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

    // 구조체 t를 0으로 초기화합니다.
    memset (t, 0, sizeof *t);

    // 스레드 상태를 BLOCKED로 설정합니다.
    t->status = THREAD_BLOCKED;

    // 스레드의 이름을 복사하고 이름 길이가 너무 길면 잘릴 수 있도록 처리합니다.
    strlcpy (t->name, name, sizeof t->name);

    // 스레드의 스택 포인터를 설정합니다. - 스택의 최상단을 가리킴
    t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);

    // 스레드의 우선 순위를 설정합니다.
    t->priority = priority;

	// 수정!!!!
	t->origin_priority = priority;
	t->wait_on_lock = NULL;
	list_init(&t->donations);

    // 스레드의 마법값(magic)을 설정합니다. (디버깅 및 오류 검사에 사용될 수 있음)
    t->magic = THREAD_MAGIC;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread.
   
   다음 스레드를 선택하고 반환하는 함수입니다. 실행 대기열(run queue)에서 스레드를 반환해야 하며,
   실행 대기열이 비어있는 경우 idle_thread를 반환해야 합니다.
   실행 대기열이 비어 있지 않으면 (현재 실행 중인 스레드가 계속 실행할 수 있다면)
   실행 대기열에서 스레드를 선택하여 반환합니다. */

static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */

// 컨텍스트 
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule().
 * 
 * 새로운 프로세스를 스케줄하는 함수입니다.
 * 이 함수를 호출하기 전에 인터럽트가 비활성화되어 있어야 합니다.
 * 이 함수는 현재 스레드의 상태를 지정된 상태(status)로 변경한 다음 다른 스레드를 찾아서 실행하도록 스위치합니다.
 * schedule() 함수 내에서 printf() 함수를 호출하는 것은 안전하지 않다는 주석도 포함되어 있습니다.
 * 함수는 스레드 스케줄링과 관련된 작업을 수행하는 중요한 함수로, 스레드 간의 전환과 스케줄링을 관리합니다.
 * */

// 파괴한거 따로 모아 놓음
// 쓰레드의 상태
static void
do_schedule(int status) {

	// 인터럽트가 꺼져있는 상태, 
	ASSERT (intr_get_level () == INTR_OFF);
	// 쓰레드 상태가 러닝중일때,
	ASSERT (thread_current()->status == THREAD_RUNNING);

	while (!list_empty (&destruction_req)) { // 비어있지 않으면 -> 비어있으면 while 종료
		// 더이상 파괴하려는 쓰레드가 없을 때까지 while문 실행 -> 
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		
		// 피지컬 페이지 -> 동적 할당 아닌 것 같음, 희생자들의 프리 시킴, FIFO, 
		palloc_free_page(victim);
	}
	// 입맛대로 쓰면 된다.
	thread_current ()->status = status;

	// 죽인 걸, 죽인 동작하지 않음
	schedule ();
}

// 왜 호출 -> 블록 해줬으니까 다시 레디큐에 있는 러닝상태로 바꿔주기
static void
schedule (void) {
	struct thread *curr = running_thread (); // 현재 실행 중인 스레드
	struct thread *next = next_thread_to_run (); // 레디 큐에서 다음에 실행할 스레드 골라서 return, (현재 round-robin)

	// 다른 쓰레드가 스케줄링 중일때는 인터럽트가 걸리면 안되니까 비활성화
	ASSERT (intr_get_level () == INTR_OFF);
	
	// 현재 쓰레드가 러닝 상태가 아니면 다음으로 넘어간다.  
	ASSERT (curr->status != THREAD_RUNNING);

	// 쓰레드인지 확인, 레디, 블럭, 죽은 거 다 넘어옴
	ASSERT (is_thread (next));

	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. 시간을 얼마냐 썼냐 -> 다음으로 넘어갔음 */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif
	if (curr != next) {
		// 커렌트 쓰레드가 죽은 쓰레드가 죽은 쓰레드를 모아 놓는 곳[죽었다고 마킹한 애들을 모아놓는 곳],
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */

		// 맨처음(메인) 쓰레드 이닛에서 호출할 떄 (맨 처음 생성된 쓰레드 initial_thread)가 아니여야 한다.
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		
		// 
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. - 새로운 스레드에 사용할 스레드 ID(tid)를 반환합니다. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}


