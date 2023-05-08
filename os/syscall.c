#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"
// #include "vm.h"
#include "riscv.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_gettimeofday(uint64 va, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	struct proc *p = curr_proc();
	// YOUR CODE
	uint64 cycle = get_cycle();
	TimeVal tmp;
	tmp.sec = cycle/(CPU_FREQ);
	tmp.usec = cycle*10/125;
	copyout(p->pagetable,va,(char *)&tmp,16);
	

	/* The code in `ch3` will leads to memory bugs*/

	// uint64 cycle = get_cycle();
	// val->sec = cycle / CPU_FREQ;
	// val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
}

pte_t *walk_1(pagetable_t pagetable, uint64 va, int alloc){
	if (va >= MAXVA)
		panic("walk");

	for (int level = 2; level > 0; level--) {
		pte_t *pte = &pagetable[PX(level, va)];
		if (*pte & PTE_V) {
			pagetable = (pagetable_t)PTE2PA(*pte);
		} else {
			if (!alloc || (pagetable = (pde_t *)kalloc()) == 0)
				return 0;
			memset(pagetable, 0, PGSIZE);
			*pte = PA2PTE(pagetable) | PTE_V | PTE_U;
		}
	}
	return &pagetable[PX(0, va)];
}

uint64 sys_sbrk(int n)
{
	uint64 addr;
        struct proc *p = curr_proc();
        addr = p->program_brk;
        if(growproc(n) < 0)
                return -1;
        return addr;
}

int mmap(void* start, unsigned long long len,int port,int flag ,int fd){
	if( len == 0 ) return 0;
	if( len > (1<<20) ) return -1;
	if( (uint64)start % 4096 ) return -1;
	// if( len % 4096 ) return -1;
	if( (port & ~0x7) != 0 ) return -1;
	if( (port & 0x7)  == 0) return -1;
	uint64 pa = (uint64)kalloc();
	// printf("%d %d",len,port);
	return mappages(curr_proc()->pagetable,(uint64)start,len,pa,PTE_U|(port<<1));
	// uint64 a, last;
	// pte_t *pte;
	// uint64 va = (uint64)(start);
	// a = PGROUNDDOWN(va);
	// last = PGROUNDDOWN(va + len - 1);
	// printf("%d",port);
	// for (;;) {
	// 	if ((pte = walk_1(curr_proc()->pagetable, a, 1)) == 0)
	// 		return -1;
	// 	if (*pte & PTE_V) {
	// 		errorf("remap");
	// 		return -1;
	// 	}
	// 	*pte = PA2PTE(pa) | (port<<1) | PTE_V | PTE_U;
	// 	if (a == last)
	// 		break;
	// 	a += PGSIZE;
	// 	pa += PGSIZE;
	// }
	// return 0;
}

int munmap(void* start, unsigned long long len){
	uint64 a, last;
	a = PGROUNDDOWN((uint64)start);
	last = PGROUNDDOWN((uint64)start + len - 1);
	for(; a < last ; a += PGSIZE ){
		if( walkaddr(curr_proc()->pagetable,a) ){
			uvmunmap(curr_proc()->pagetable,(uint64)start,1,1);
		}else return -1;
	}
	return 0;
}
// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/
uint64 sys_task_info( uint64 va ){
	TaskInfo ta;
	uint64 cycle = get_cycle();
	ta.status = Running;
	for(int i = 0; i < MAX_SYSCALL_NUM; i++){
		ta.syscall_times[i] = curr_proc()->syscall_times[i];
	}
	ta.time = cycle/(CPU_FREQ/1000)-curr_proc()->stime;
	copyout(curr_proc()->pagetable,va,(char *)&ta,sizeof(ta));
	return 0;
}

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	curr_proc()->syscall_times[id] += 1;
	/*
	* LAB1: you may need to update syscall counter for task info here
	*/
	// curr_proc()->
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday(args[0], args[1]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
	case SYS_task_info:
		ret = sys_task_info(args[0]);
		break;
	case SYS_mmap:
		ret = mmap((void *)args[0],args[1],args[2],0,0);
		break;
	case SYS_munmap:
		ret = munmap((void *)args[0],args[1]);
		break;
	/*
	* LAB1: you may need to add SYS_taskinfo case here
	*/
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
// git add .; git commit -m "update"; git push