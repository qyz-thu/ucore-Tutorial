#include "defs.h"
#include "syscall_ids.h"
#include "trap.h"
#include "proc.h"
#include "riscv.h"

uint64 sys_write(int fd, uint64 va, uint len) {
    if (fd != 1)
        return -1;
    struct proc *p = curr_proc();
    char str[200];
    int size = copyinstr(p->pagetable, str, va, MIN(len, 200));
    for(int i = 0; i < size; ++i) {
        console_putchar(str[i]);
    }
    return size;
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

uint64 sys_read(int fd, uint64 va, uint64 len) {
    if (fd != 0)
        return -1;
    struct proc *p = curr_proc();
    char str[200];
    for(int i = 0; i < len; ++i) {
        int c = console_getchar();
        str[i] = c;
    }
    copyout(p->pagetable, va, str, len);
    return len;
}

uint64 sys_exit(int code) {
    exit(code);
    return 0;
}

uint64 sys_sched_yield() {
    yield();
    return 0;
}

uint64 sys_getpid() {
    return curr_proc()->pid;
}

uint64 sys_clone() {
    // info("fork!\n");
    return fork();
}

uint64 sys_exec(uint64 va) {
    struct proc* p = curr_proc();
    char name[200];
    copyinstr(p->pagetable, name, va, 200);
    // info("sys_exec %s\n", name);
    return exec(name);
}

uint64 sys_wait(int pid, uint64 va) {
    struct proc* p = curr_proc();
    int* code = (int*)useraddr(p->pagetable, va);
    return wait(pid, code);
}

uint64 sys_times() {
    return get_time_ms();
}

uint64 sys_spawn(char* filename) {
    struct proc *p = curr_proc();

    char* k_filename = (char*) useraddr(p->pagetable, (uint64) filename);
    int child_id;
    struct proc *np;


    // use codes from fork() and exec()
    // Allocate process.
    if((np = allocproc()) == 0){
        panic("allocproc\n");
    }
    // Copy user memory from parent to child.
    if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
        panic("uvmcopy\n");
    }

    // Cause fork to return 0 in the child.
    np->trapframe->a0 = 0;
    child_id = np->pid;
    np->parent = p;

    if(child_id < 0)
        return -1;

    proc_freepagetable(np->pagetable, np->sz);
    np->sz = 0;
    np->pagetable = proc_pagetable(np);
    if(np->pagetable == 0){
        panic("np->pagetable == 0");
    }
    int file_id = get_id_by_name(k_filename);
    loader(file_id, np);
    np->state = RUNNABLE;
    return child_id;
}

void syscall() {
    struct proc *p = curr_proc();
    struct trapframe *trapframe = p->trapframe;
    int id = trapframe->a7, ret;
    uint64 args[7] = {trapframe->a0, trapframe->a1, trapframe->a2, trapframe->a3, trapframe->a4, trapframe->a5, trapframe->a6};
    trace("syscall %d args:%p %p %p %p %p %p %p\n", id, args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
    switch (id) {
        case SYS_write:
            ret = sys_write(args[0], args[1], args[2]);
            break;
        case SYS_read:
            ret = sys_read(args[0], args[1], args[2]);
            break;
        case SYS_exit:
            ret = sys_exit(args[0]);
            break;
        case SYS_sched_yield:
            ret = sys_sched_yield();
            break;
        case SYS_getpid:
            ret = sys_getpid();
            break;
        case SYS_clone: // SYS_fork
            ret = sys_clone();
            break;
        case SYS_execve:
            ret = sys_exec(args[0]);
            break;
        case SYS_wait4:
            ret = sys_wait(args[0], args[1]);
            break;
        case SYS_times:
            ret = sys_times();
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
        case SYS_SPAWN:
            ret = sys_spawn((char* )args[0]);
            break;
        default:
            ret = -1;
            warn("unknown syscall %d\n", id);
    }
    trapframe->a0 = ret;
    trace("syscall ret %d\n", ret);
}