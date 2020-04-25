#include <assert.h>
#include <list.h>
#include <proc.h>
#include <sched.h>
#include <sync.h>

void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
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
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        do {
            if ((le = list_next(le)) != &proc_list) {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE) {
                    break;
                }
            }
        } while (le != last);
        if (next == NULL || next->state != PROC_RUNNABLE) {
            next = idleproc;
        }
        next->runs++;
        if (next != current) {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}
