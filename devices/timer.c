#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
// 운영체제가 부팅되고 나서 지난 타이머 틱의 수를 나타내며, 시스템의 시간 측정과 관련된 정보 중 하나
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);

// 타이머 인터럽트: 주기적으로 발생하여 스케줄러가 실행 중인 스레드를 중단하고
// 다음 스레드를 실행하도록 하는 인터럽트입니다.

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt.

   8254 Programmable Interval Timer (PIT)를 초당 PIT_FREQ 번의 인터럽트를 발생하도록
   설정하고 해당하는 인터럽트를 등록합니다.

   8254 PIT는 컴퓨터 아키텍처에서 사용되는 타이머 칩입니다.
   이 칩은 시스템에서 정밀한 타이밍 및 인터럽트를 관리하는 데 사용됩니다.
   주로 주기적인 타이머 인터럽트 및 다른 시간 관련 작업에 활용됩니다.
   위의 문장은 8254 PIT를 설정하여 초당 일정 횟수의 인터럽트를 발생하도록 하는 작업을 수행한다는 내용을 나타냅니다.
*/

// 8254 PIT를 Rate Generator 모드로 설정하고,
// 주어진 주기(count 값)로 타이머를 초기화하며,
// 이를 커널 인터럽트로 등록하는 역할을 합니다.
// 이를 통해 커널은 일정한 주기로 타이머 인터럽트를 받아 시간 관련 작업을 수행
void
timer_init (void) {
	/* 8254 input frequency divided by TIMER_FREQ, rounded to nearest.
	   8254 입력 주파수를 TIMER_FREQ로 나눈 값, 반올림한 값입니다. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ; // TIMER_FREQ = 100;

	// 8254 PIT의 컨트롤 레지스터(Command Word, CW)에 값을 출력하여 타이머 설정을 지정
	outb (0x43, 0x34);    /* CW: counter 0, LSB then MSB, mode 2, binary. */
	// "counter 0": PIT의 카운터 0을 설정하겠다는 의미.
	// "LSB then MSB": 카운터 값을 LSB(Low-Byte)와 MSB(High-Byte)로 나누어 입력할 것.
	// "mode 2": PIT의 작동 모드를 2로 설정 (Rate Generator 모드).
	// "binary": 카운터 값을 이진(binary)으로 처리하겠다는 의미.

	outb (0x40, count & 0xff);	// PIT의 카운터 0의 LSB(Low-Byte)에 count 값을 설정
	outb (0x40, count >> 8);	// PIT의 카운터 0의 MSB(High-Byte)에 count 값을 설정

	intr_register_ext (0x20, timer_interrupt, "8254 Timer");
	//  PIT에서 발생하는 인터럽트를 등록
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
// 운영 체제 부팅 이후의 타이머 틱 수를 반환합니다.
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable (); // 인터럽트가 발생하지않는 상태에서 tick수를 안전하게 읽어올 수 있음
	int64_t t = ticks;
	intr_set_level (old_level); // 이전에 작성한 인터럽트 레벨로 복원, 인터럽트 다시 활성화
	barrier ();
	return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
// 특정시간 이후로 경과된 시간(ticks) 를 반환
int64_t
timer_elapsed (int64_t then) { // 맨 처음은 0
	return timer_ticks () - then; // 현재 ticks 반환, then은 start -> 최대 ticks에 도달하면 0이 되겠지, 
}

/* Suspends execution for approximately TICKS timer ticks. - TICKS 타이머 틱에 대략적으로 대기 */
// timer_sleep만 고치면 된다.
void
timer_sleep (int64_t ticks) { // 우리가 현재 슬립 시켜주고 싶은 ticks(ex. 5초)가 최소 5초동안 재움 
	// timer_ticks 호출 값을 현재 ticks(ex. 1초 5 ticks 동안 cpu 점유 가능) 지금 얼마동안 돌아가는 지 -> 
	int64_t start = timer_ticks () + ticks;

	ASSERT (intr_get_level () == INTR_ON);	// 인터럽트 상태인지 확인, INTR_ON 상태면, yield 할 수 있음 
	thread_sleep(start); // 자고있는 쓰레드를 깨워라 시간되면 깨워라, 잠든 
	// while (timer_elapsed (start) < ticks)	// start로 부터 얼마나 시간이 지났는 지 반환해서 ticks랑 비교해서 
	// thread_yield ();					// 양보를 함 -> 스케줄링이 일어나면서, 시간을 씀, 원래 쓰레드로 스위칭
}
// 바로 wake하지 말고, 대기하고 레디

/* Suspends execution for approximately MS milliseconds. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++;
	thread_tick ();
	thread_wake (ticks);

}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) {
	/* Wait for a timer tick. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* Run LOOPS loops. */
	start = ticks;
	busy_wait (loops);

	/* If the tick count changed, we iterated too long. */
	barrier ();
	return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes. */
		timer_sleep (ticks);
	} else {
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}
