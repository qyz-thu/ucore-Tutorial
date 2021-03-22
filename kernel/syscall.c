#include "defs.h"
#include "syscall_ids.h"
#include "trap.h"
#include "proc.h"

#define min(a, b) a < b ? a : b;


uint64 sys_write(int fd, char *str, uint len) {
    // printf("fd = %d, buf = %p, len = %d\n", fd, str, len);
    struct proc* p = curr_proc();
    if (fd != 1)
        return -1;
    uint64 ustack_s = (uint64) p->ustack;
    uint64 ustack_e = (uint64) p->ustack + PAGE_SIZE;
    uint64 usec_s = BASE_ADDRESS + (p->pid - 1) * MAX_APP_SIZE;
    uint64 usec_e = usec_s + MAX_APP_SIZE;

    if ((uint64) str + len < ustack_e && (uint64) str >= ustack_s)
    {
        // printf("in user stack\n");
        ;   // OK
    }
    else if ((uint64) str + len < usec_e && (uint64) str >= usec_s)
    {
        // in user section
        ; // OK
    }
    else
        return -1;

    int size = min(strlen(str), len);
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
    ts->sec = ms / 1000;
    ts->usec = (ms % 1000) * 1000;
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

void syscall() {
    struct trapframe *trapframe = curr_proc()->trapframe;
    int id = trapframe->a7, ret;
    // printf("syscall %d\n", id);
    uint64 args[7] = {trapframe->a0, trapframe->a1, trapframe->a2, trapframe->a3, trapframe->a4, trapframe->a5, trapframe->a6};
    switch (id) {
        case SYS_write:
            ret = sys_write(args[0], (char *) args[1], args[2]);
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
            ret = sys_gettime((struct TimeVal*) args[0], args[1]);
            break;
        default:
            ret = -1;
            printf("unknown syscall %d\n", id);
    }
    trapframe->a0 = ret;
    // printf("syscall ret %d\n", ret);
}
