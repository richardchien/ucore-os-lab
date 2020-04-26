#include <assert.h>
#include <default_sched.h>
#include <list.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <sync.h>

// the list of timer
static list_entry_t timer_list;

static struct sched_class *sched_class;

static struct run_queue *rq;

static inline void sched_class_enqueue(struct proc_struct *proc) {
    if (proc != idleproc) {
        sched_class->enqueue(rq, proc);
    }
}

static inline void sched_class_dequeue(struct proc_struct *proc) {
    sched_class->dequeue(rq, proc);
}

static inline struct proc_struct *sched_class_pick_next(void) {
    return sched_class->pick_next(rq);
}

void sched_class_proc_tick(struct proc_struct *proc) {
    if (proc != idleproc) {
        sched_class->proc_tick(rq, proc);
    } else {
        proc->need_resched = 1;
    }
}

static struct run_queue __rq;

void sched_init(void) {
    list_init(&timer_list);

    sched_class = &default_sched_class;

    rq = &__rq;
    rq->max_time_slice = MAX_TIME_SLICE;
    sched_class->init(rq);

    cprintf("sched class: %s\n", sched_class->name);
}

void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
            if (proc != current) {
                sched_class_enqueue(proc);
            }
        } else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}

/**
 * 打印进程列表, 用于调试.
 */
static void dump_proc_list() {
    list_entry_t *le = &proc_list;
    cprintf("proc_list");
    while ((le = list_next(le)) != &proc_list) {
        struct proc_struct *p = le2proc(le, list_link);
        cprintf(" -> (name: %s, pid: %d, state: %d)", p->name, p->pid, p->state);
    }
    cprintf(" -> proc_list\n");
}

void schedule(void) {
    dump_proc_list();

    bool intr_flag;
    struct proc_struct *next;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        if (current->state == PROC_RUNNABLE) {
            sched_class_enqueue(current);
        }
        if ((next = sched_class_pick_next()) != NULL) {
            sched_class_dequeue(next);
        }
        if (next == NULL) {
            next = idleproc;
        }
        next->runs++;
        if (next != current) {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}
