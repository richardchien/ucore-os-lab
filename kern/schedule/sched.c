#include <assert.h>
#include <default_sched.h>
#include <default_sched_stride.h>
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

static void sched_class_proc_tick(struct proc_struct *proc) {
    if (proc != idleproc) {
        sched_class->proc_tick(rq, proc);
    } else {
        proc->need_resched = 1;
    }
}

static struct run_queue __rq;

void sched_init(void) {
    list_init(&timer_list);

    // sched_class = &default_sched_class;
    sched_class = &default_sched_class_stride;

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
    // dump_proc_list();

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

// add timer to timer_list
void add_timer(timer_t *timer) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        assert(timer->expires > 0 && timer->proc != NULL);
        assert(list_empty(&(timer->timer_link)));
        list_entry_t *le = list_next(&timer_list);
        while (le != &timer_list) {
            timer_t *next = le2timer(le, timer_link);
            if (timer->expires < next->expires) { // 找到比当前要插入的 timer 剩余时间长的
                next->expires -= timer->expires; // 去掉其重合时间, break 后将当前 timer 插入在它前面
                break;
            }
            timer->expires -= next->expires; // next 比 timer 剩余时间短, 则去掉重合时间
            le = list_next(le); // 并继续向后挪
        }
        list_add_before(le, &(timer->timer_link));
    }
    local_intr_restore(intr_flag);
}

// del timer from timer_list
void del_timer(timer_t *timer) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (!list_empty(&(timer->timer_link))) {
            if (timer->expires != 0) {
                list_entry_t *le = list_next(&(timer->timer_link));
                if (le != &timer_list) {
                    timer_t *next = le2timer(le, timer_link);
                    next->expires += timer->expires;
                }
            }
            list_del_init(&(timer->timer_link));
        }
    }
    local_intr_restore(intr_flag);
}

// call scheduler to update tick related info, and check the timer is expired? If expired, then wakup proc
void run_timer_list(void) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        list_entry_t *le = list_next(&timer_list);
        if (le != &timer_list) {
            timer_t *timer = le2timer(le, timer_link);
            assert(timer->expires != 0);
            timer->expires--;
            while (timer->expires == 0) { // 剩余 tick 耗尽
                le = list_next(le);
                struct proc_struct *proc = timer->proc;
                if (proc->wait_state != 0) {
                    assert(proc->wait_state & WT_INTERRUPTED);
                } else {
                    warn("process %d's wait_state == 0.\n", proc->pid);
                }
                wakeup_proc(proc); // 则唤醒进程
                del_timer(timer);
                if (le == &timer_list) {
                    break;
                }
                timer = le2timer(le, timer_link);
            }
        }
        sched_class_proc_tick(current); // 给调度器喂 tick
    }
    local_intr_restore(intr_flag);
}
