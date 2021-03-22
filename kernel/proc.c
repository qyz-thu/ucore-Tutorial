#include "defs.h"
#include "proc.h"
#include "trap.h"

struct proc pool[NPROC];
char kstack[NPROC][PAGE_SIZE];
__attribute__ ((aligned (4096))) char ustack[NPROC][PAGE_SIZE];
char trapframe[NPROC][PAGE_SIZE];

extern char boot_stack_top[];
struct proc* current_proc;
struct proc idle;


struct proc* curr_proc() {
    return current_proc;
}

void
procinit(void)
{
    struct proc *p;
    for(p = pool; p < &pool[NPROC]; p++) {
        p->state = UNUSED;
        p->kstack = (uint64)kstack[p - pool];
        p->ustack = (uint64)ustack[p - pool];
        p->trapframe = (struct trapframe*)trapframe[p - pool];
    }
    idle.kstack = (uint64)boot_stack_top;
    idle.pid = 0;
    idle.killed = 0;
}

int allocpid() {
    static int PID = 1;
    return PID++;
}

struct proc* allocproc(void)
{
    struct proc *p;
    for(p = pool; p < &pool[NPROC]; p++) {
        if(p->state == UNUSED) {
            goto found;
        }
    }
    return 0;

    found:
    p->pid = allocpid();
    p->state = USED;
    p->ttl = 500;
    p->priority = 16;
    p->stride = 0;

    memset(&p->context, 0, sizeof(p->context));
    memset(p->trapframe, 0, PAGE_SIZE);
    memset((void*)p->kstack, 0, PAGE_SIZE);
    p->context.ra = (uint64)usertrapret;
    p->context.sp = p->kstack + PAGE_SIZE;
    return p;
}

void
scheduler(void)
{
    struct proc *p;

    for(;;){
        struct proc* target_proc = pool - 1;
        uint64 least_stride = 2147483647;
        for(p = pool; p < &pool[NPROC]; p++) {
            if(p->state == RUNNABLE && p->stride < least_stride) {
                least_stride = p->stride;
                target_proc = p;
            }
        }
        if (target_proc != pool - 1)
        {
            // printf("switch process.\n");
            target_proc->state = RUNNING;
            current_proc = target_proc;
            target_proc->ttl--;
            target_proc->stride += BIGSTRIDE / target_proc->priority;
            swtch(&idle.context, &target_proc->context);
        }
        else
        {
            // for (p = pool; p < &pool[NPROC]; p++) {
            //     printf("proc %d, state %d, stride %d, ttl %d\n", p->pid, p->state, p->stride, p->ttl);
            // }

            panic("no process found.\n");
        }
    }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
    struct proc *p = curr_proc();
    if(p->state == RUNNING)
        panic("sched running");
    swtch(&p->context, &idle.context);
}

// Give up the CPU for one scheduling round.
void yield(void)
{
    if (current_proc->ttl > 0)
        current_proc->state = RUNNABLE;
    else
    {
        // current_proc->state = ZOMBIE;
        exit(-1);
    }
    sched();
}

void exit(int code) {
    struct proc *p = curr_proc();
    // printf("proc %d exit with %d\n", p->pid, code);
    p->state = UNUSED;
    finished();
    sched();
}