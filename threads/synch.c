/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);	// 리스트를 초기화 해줌, value 넣어주고, waiters를 초기화 -> waiters: 쓰레드들이 대기하고 있는 리스트
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */

// S = PV
// 무조건 호출 -> 세마 val이 0이면 -> 
void

sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	// 둘다 아니면 인터럽트 비활성화, value가 0이면 안으로 들어가
	// 다른 인터럽트의 간섭을 방해를 막기 위해
	old_level = intr_disable ();
	while (sema->value == 0) { // 0이면 계속 돌아, value가 1이 되면 빠져나감
		// thread_current(), 현재 러닝중인 waiters리스트 맨뒤에 넣고 블락 시켜줌
		list_push_back (&sema->waiters, &thread_current ()->elem);  // w 리스트에서 나온, thread_current ()->elem 나온 element를 넣어준다.  
		thread_block ();		// 현재 러닝중인 쓰레드를 블럭으로 만들어준다.
	}

	// while, 현재 실행중인 쓰레드(러닝 중) 자기 자신을 wait 리스트에 넣고 자기자신을 블럭 시킴 그러면 해당 쓰레드는 블럭된 상태가 되고,
	// 다른 쓰레드로 스케줄링이 되서 계속 시간이 지나다가 세마업이 호출되는 순간, 세마업을 가, wait list(블럭된 쓰레드중 맨앞에 unblock)에 있는 애를 unblock 시키고, 세마포어값을 1개 증가시킴
	// 블럭 된 애들중 하나를 러닝으로 바꿔줌, val를 up, 그런데 이때, 러닝상태로 바꿔준, 애가 세마다운에서 돌고있는 애라면, value를 감소시키고,

	sema->value--;
	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
// 내리는 것을 시도, 못내리면 0 리턴, 내릴수 있으면 1, sema_try_down는 쓰지말래(안 쓰는게 좋다.)
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler.
   
   세마포어(SEMA)에 대한 'Up' 또는 'V' 연산입니다. SEMA의 값을 증가시키고,
   SEMA를 기다리고 있는 스레드 중 하나를 깨우는 역할을 합니다.
   이 함수는 인터럽트 핸들러에서 호출될 수 있습니다. */

// 이 함수는 스레드 간의 대기(wait)와 신호(signal)을 이용하여 스레드의 실행을 동기화하는데 사용되며,
// 세마포어의 값을 증가시킴으로써 대기 중인 스레드 중 하나를 깨워서 실행 가능한 상태로 만들 수 있습니다.
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable (); // 인터럽트를 비활성화하고, 이전 인터럽트 상태를 반환
      if (!list_empty (&sema->waiters)) // !(waiters[begin] == waiters[end]), waiters 리스트에 값이 있다면, 
		thread_unblock (list_entry (list_pop_front (&sema->waiters), // 세마의 waiters 리스트 맨 앞에서 pop, 
					struct thread, elem));
      // thread_unblock() - 꺼내온 스레드를 준비 상태(unblock)로 전환합니다.

	sema->value++; // sema의 value 증가
	intr_set_level (old_level);   // 이전 인터럽트 상태로 복원합니다.
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock.
   
   LOCK을 초기화합니다. LOCK은 어떤 시점에서도 최대 하나의 스레드만 보유할 수 있는 락(lock)입니다.
   우리의 락(lock)은 '재귀적'이지 않으며,
   즉, 현재 락(lock)을 보유한 스레드가 해당 락(lock)을 다시 획득하려고 시도하는 것은 오류입니다. 

   락(lock)은 초기 값이 1인 세마포어(sempahore)의 특수화된 형태입니다.
   락(lock)과 이러한 세마포어 간의 차이점은 두 가지입니다. 
   첫째, 세마포어는 1보다 큰 값을 가질 수 있지만 락(lock)은 한 번에 하나의 스레드만 소유할 수 있습니다.
   둘째, 세마포어는 소유자가 없으며, 한 스레드가 세마포어를 'down'하고 다른 스레드가 그것을 'up'할 수 있지만,
   락(lock)의 경우 동일한 스레드가 락(lock)을 획득하고 해제해야 합니다.
   이러한 제약 사항이 불편하다면 세마포어가 락(lock) 대신 사용되어야 하는 좋은 신호입니다.
   */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL; // lock->holder, lock을 소유한 스레드가 아님 NULL
   
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);     // lock이 NULL이 아니면
	ASSERT (!intr_context ()); // 외부 인터럽트 처리 여부 
	ASSERT (!lock_held_by_current_thread (lock));   // 현재 쓰레드가 lock->holder랑 같은 애가 아니면 

	sema_down (&lock->semaphore);    // sema_down, 
	lock->holder = thread_current ();      // main
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) { 
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	lock->holder = NULL;
	sema_up (&lock->semaphore);
   thread_yield();
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();    // lock->holder가 현재 돌고있는 쓰레드랑 같은 쓰레드면,
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	list_push_back (&cond->waiters, &waiter.elem);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
