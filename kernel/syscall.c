#include "defs.h"
#include "syscall_ids.h"
#include "trap.h"
#include "proc.h"
#include "riscv.h"

#define min(a, b) a < b ? a : b;

uint64 sys_write(int fd, char *addr, uint len) {
    if (fd != 1)
        return -1;
    struct proc *p = curr_proc();
    char str[200];
    int size = copyinstr(p->pagetable, str, (uint64) addr, MIN(len, 200));
    // printf("size = %d\n", size);
    for(int i = 0; i < size; ++i) {
        console_putchar(str[i]);
    }
    return size;
}

uint64 sys_exit(int code) {
    exit(code);
    return 0;
}

uint64 sys_sched_yield() {
    yield();
    return 0;
}

uint64 sys_gettime(struct TimeVal* ts, int tz)
{
    uint64 ms = get_time_ms();
    struct proc* p = curr_proc();
    pagetable_t pg = p -> pagetable;
    struct TimeVal* phy_ts = (struct TimeVal*) useraddr(pg, (uint64) ts);
    
    phy_ts->sec = ms / 1000;
    phy_ts->usec = (ms % 1000) * 1000;
    return 0;
}

uint64 sys_setpriority(uint64 prio) {
    // printf("%d\n", prio);
    if(prio >= 2 && prio <= 2147483647) {
        struct proc* p = curr_proc();
        p -> priority = prio;
        return prio;
    }
    return -1;
}

int sys_mmap(uint64 start, uint64 len, int port) {
    if ((port & ~0x7) != 0 || (port & 0x7) == 0)    // illegal port
        return -1;
    if (start % PGSIZE != 0)    // unaligned start addr
        return -1;
    if (len > (1 << 30))    // too large
        return -1;
    if (start < 0x2000 || start + PGROUNDUP(len) > 0x3fffffe000)     // out of range
        return -1;
    struct proc *p = curr_proc();
    pagetable_t pg = p -> pagetable;
    // check if the memory is already allocated
    for (uint64 addr = start; addr < start + PGROUNDUP(len); addr += PGSIZE)
        if (useraddr(pg, addr) != 0)
            return -1;
    mappages(pg, start, len, (uint64)kalloc(), (port << 1) | PTE_U);

    return PGROUNDUP(len);
}

int sys_munmap(uint64 start, uint64 len) {
    if (start % PGSIZE != 0)    // unaligned start addr
        return -1;
    if (len > (1 << 30))    // too large
        return -1;
    if (start < 0x2000 || start + PGROUNDUP(len) > 0x3fffffe000)     // out of range
        return -1;
    struct proc *p = curr_proc();
    pagetable_t pg = p -> pagetable;
    // check if the memory haven't been allocated
    for (uint64 addr = start; addr < start + PGROUNDUP(len); addr += PGSIZE)
        if (useraddr(pg, addr) == 0)
            return -1;

    uvmunmap(pg, start, PGROUNDUP(len) / PGSIZE, 0);
    return PGROUNDUP(len);
}

void syscall() {
    struct trapframe *trapframe = curr_proc()->trapframe;
    int id = trapframe->a7, ret;
    uint64 args[6] = {trapframe->a0, trapframe->a1, trapframe->a2, trapframe->a3, trapframe->a4, trapframe->a5};
    // printf("syscall %d args:%p %p %p %p %p %p\n", id, args[0], args[1], args[2], args[3], args[4], args[5]);
    switch (id) {
        case SYS_write:
            ret = sys_write(args[0], (char *) args[1], args[2]);
            printf("\n");
            break;
        case SYS_exit:
            ret = sys_exit(args[0]);
            break;
        case SYS_sched_yield:
            ret = sys_sched_yield();
            break;
        case SYS_setpriority:
            ret = sys_setpriority(args[0]);
            break;
        case SYS_gettimeofday:
            ret = sys_gettime((struct TimeVal*)args[0], args[1]);
            break;
        case SYS_mmap:
            ret = sys_mmap(args[0], args[1], args[2]);
            break;
        case SYS_munmap:
            ret = sys_munmap(args[0], args[1]);
            break;
        default:
            ret = -1;
            // printf("unknown syscall %d\n", id);
    }
    trapframe->a0 = ret;
    // printf("syscall ret %d\n", ret);
}
