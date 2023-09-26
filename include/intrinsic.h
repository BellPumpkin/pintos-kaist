#ifndef INSTRINSIC_H
#include "threads/mmu.h"

/* Store the physical address of the page directory into CR3
   aka PDBR (page directory base register).  This activates our
   new page tables immediately.  See [IA32-v2a] "MOV--Move
   to/from Control Registers" and [IA32-v3a] 3.7.5 "Base Address
   of the Page Directory". */
__attribute__((always_inline))
static __inline void lcr3(uint64_t val) {
	__asm __volatile("movq %0, %%cr3" : : "r" (val));
}

__attribute__((always_inline))
static __inline void lgdt(const struct desc_ptr *dtr) {
	__asm __volatile("lgdt %0" : : "m" (*dtr));
}
// static __inline: 이 함수는 정적(static)으로 선언되고, 인라인(inline) 어셈블리 코드를 사용합니다.
// 이렇게 하면 함수 호출 오버헤드가 감소하고, 특정 어셈블리 명령어를 직접 실행할 수 있습니다.

// void lgdt(const struct desc_ptr *dtr): lgdt 함수는 struct desc_ptr 타입의 포인터 dtr을 매개변수로 받습니다.
// 이 포인터는 GDT를 가리키는 구조체 desc_ptr를 가리킵니다.

// __asm __volatile("lgdt %0" : : "m" (*dtr)): 이 부분은 인라인 어셈블리 코드를 나타냅니다.
// lgdt 어셈블리 명령어를 실행하여 GDT 레지스터에 새로운 GDT 주소를 설정합니다.

// "lgdt %0": "lgdt"는 어셈블리 명령어로, GDT 레지스터에 새로운 GDT 주소를 설정하는 역할을 합니다.
// %0은 입력 제약(constraint)으로, "m" 제약에 해당하는 *dtr을 가리킵니다.
// 즉, *dtr의 값이 GDT 주소를 나타내며, 이 주소가 "lgdt" 명령어에 전달됩니다.

// 이 함수는 lgdt 어셈블리 명령어를 사용하여 GDT 레지스터를 설정합니다.
// GDT는 x86 아키텍처에서 세그먼트 디스크립터를 저장하는 테이블이며, 이 함수를 통해 GDT를 업데이트할 수 있습니다.
// GDT를 변경하면 메모리 보호 및 세그먼트 설정과 같은 시스템 레벨의 설정을 조절할 수 있습니다.


__attribute__((always_inline))
static __inline void lldt(uint16_t sel) {
	__asm __volatile("lldt %0" : : "r" (sel));
}

__attribute__((always_inline))
static __inline void ltr(uint16_t sel) {
	__asm __volatile("ltr %0" : : "r" (sel));
}

__attribute__((always_inline))
static __inline void lidt(const struct desc_ptr *dtr) {
	__asm __volatile("lidt %0" : : "m" (*dtr));
}

__attribute__((always_inline))
static __inline void invlpg(uint64_t addr) {
	__asm __volatile("invlpg (%0)" : : "r" (addr) : "memory");
}

__attribute__((always_inline))
static __inline uint64_t read_eflags(void) {
	uint64_t rflags;
	__asm __volatile("pushfq; popq %0" : "=r" (rflags));
	return rflags;
}

__attribute__((always_inline))
static __inline uint64_t rcr3(void) {
	uint64_t val;
	__asm __volatile("movq %%cr3,%0" : "=r" (val));
	return val;
}

__attribute__((always_inline))
static __inline uint64_t rrax(void) {
	uint64_t val;
	__asm __volatile("movq %%rax,%0" : "=r" (val));
	return val;
}

__attribute__((always_inline))
static __inline uint64_t rrdi(void) {
	uint64_t val;
	__asm __volatile("movq %%rdi,%0" : "=r" (val));
	return val;
}

__attribute__((always_inline))
static __inline uint64_t rrsi(void) {
	uint64_t val;
	__asm __volatile("movq %%rsi,%0" : "=r" (val));
	return val;
}

__attribute__((always_inline))
static __inline uint64_t rrdx(void) {
	uint64_t val;
	__asm __volatile("movq %%rdx,%0" : "=r" (val));
	return val;
}

__attribute__((always_inline))
static __inline uint64_t rr10(void) {
	uint64_t val;
	__asm __volatile("movq %%r10,%0" : "=r" (val));
	return val;
}

__attribute__((always_inline))
static __inline uint64_t rr8(void) {
	uint64_t val;
	__asm __volatile("movq %%r8,%0" : "=r" (val));
	return val;
}

__attribute__((always_inline))
static __inline uint64_t rr9(void) {
	uint64_t val;
	__asm __volatile("movq %%r9,%0" : "=r" (val));
	return val;
}

__attribute__((always_inline))
static __inline uint64_t rrcx(void) {
	uint64_t val; // 이 변수는 rrsp 함수 내에서 rsp 값의 저장을 위해 사용됩니다.
	__asm __volatile("movq %%rcx,%0" : "=r" (val));
	return val;
}

// 이 함수는 x86 아키텍처에서 스택 포인터 rsp 값을 읽어와서 반환하는 역할
__attribute__((always_inline))
static __inline uint64_t rrsp(void) {
	uint64_t val;
	__asm __volatile("movq %%rsp,%0" : "=r" (val));
	return val;
}

// __attribute__((always_inline)): 이 어트리뷰트는 함수가 항상 인라인으로 처리되어야 함을 나타냅니다.
// 컴파일러에게 함수를 인라인으로 처리하도록 강제합니다.

// static __inline uint64_t rrsp(void): rrsp 함수는 스택 포인터 rsp 값을 반환하므로 uint64_t 타입의 값을 반환합니다.
// static은 함수의 정적 선언을 나타내며, __inline은 인라인 함수임을 나타냅니다.

// uint64_t val;: val이라는 uint64_t 타입의 변수를 선언합니다.
// 이 변수는 rrsp 함수 내에서 rsp 값의 저장을 위해 사용됩니다.

// __asm __volatile("movq %%rsp,%0" : "=r" (val));: 인라인 어셈블리 코드를 사용하여 rsp 레지스터의 값을 val 변수에 읽어옵니다.
// movq 어셈블리 명령어는 64비트 값을 이동시키는데 사용됩니다.
// %%rsp는 rsp 레지스터를 가리키고, %0은 출력 제약(constraint)으로 val 변수에 대한 참조를 나타냅니다.

// return val;: 읽어온 rsp 값인 val을 반환합니다.

__attribute__((always_inline))
static __inline uint64_t rcr2(void) {
	uint64_t val;
	__asm __volatile("movq %%cr2,%0" : "=r" (val));
	return val;
}

__attribute__((always_inline))
static __inline void write_msr(uint32_t ecx, uint64_t val) {
	uint32_t edx, eax;
	eax = (uint32_t) val;
	edx = (uint32_t) (val >> 32);
	__asm __volatile("wrmsr"
			:: "c" (ecx), "d" (edx), "a" (eax) );
}

#endif /* intrinsic.h */
