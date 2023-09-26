#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore {
	unsigned value;             /* Current value. */
	struct list waiters;        /* List of waiting threads. 기다리는 애들 */
};								// 만약 자원이 이미 다른 스레드에 의해 사용 중이라면 스레드는 기다리게 됩니다. 

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* Lock. - 공유 자원을 하나의 쓰레드가 사용하고 있을 때, 다른 쓰레드가 공유 자원을 사용하지 못하도록 제한을 거는 것 */
struct lock {
	struct thread *holder;      /* Thread holding lock (for debugging). - lock을 소유한 스레드 (디버깅을 위한) */
	struct semaphore semaphore; /* Binary semaphore controlling access. - 접근을 제어하는 이진 세마포어 */
};

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. */
struct condition {
	struct list waiters;        /* List of waiting threads. */
};

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* Optimization barrier.
 *
 * The compiler will not reorder operations across an
 * optimization barrier.  See "Optimization Barriers" in the
 * reference guide for more information.
 * 
 * 최적화 바리어.
 * 컴파일러는 최적화 바리어를 통과하는 연산들을 재배치(reorder)하지 않습니다.
 * 자세한 정보는 레퍼런스 가이드의 "최적화 바리어(Optimization Barriers)" 섹션을 참조
*/
#define barrier() asm volatile ("" : : : "memory")
// 매크로는 코드 최적화를 방해하고, 컴파일러에게 코드의 순서나 메모리 접근을 변경하지 말라고 지시

#endif /* threads/synch.h */
