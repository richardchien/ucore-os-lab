#include <assert.h>
#include <kmalloc.h>
#include <monitor.h>
#include <stdio.h>

// Initialize monitor.
void monitor_init(monitor_t *mtp, size_t num_cv) {
    int i;
    assert(num_cv > 0);
    mtp->next_count = 0;
    sem_init(&(mtp->mutex), 1); // unlocked
    sem_init(&(mtp->next), 0);
}

// Lock monitor procedure.
void monitor_lock(monitor_t *mtp) {
    down(&(mtp->mutex));
}

// Unlock monitor procedure.
void monitor_unlock(monitor_t *mtp) {
    if (mtp->next_count > 0) {
        up(&(mtp->next)); // 唤醒由于 monitor_yield 而等待的进程
    } else {
        up(&(mtp->mutex)); // 唤醒由于互斥锁而等待进入管程的进程
    }
}

// Yield the ownership of monitor, to let the newly waked-up process run.
void monitor_yield(monitor_t *mtp) {
    mtp->next_count++;
    down(&mtp->next); // 等待被唤醒者离开(unlock)管程
    mtp->next_count--;
}

// Initialize condition variable.
void cond_init(condvar_t *cvp) {
    cvp->count = 0;
    sem_init(&cvp->sem, 0);
}

// Unlock one of threads waiting on the condition variable.
void cond_signal(condvar_t *cvp, monitor_t *mtp) {
    // LAB7 EXERCISE1: YOUR CODE
    cprintf("cond_signal begin: cvp %x, cvp->count %d, mtp->next_count %d\n", cvp, cvp->count, mtp->next_count);
    /*
     *      cond_signal(cv) {
     *          if(cv.count>0) {
     *             mt.next_count ++;
     *             signal(cv.sem);
     *             wait(mt.next);
     *             mt.next_count--;
     *          }
     *       }
     */
    if (cvp->count > 0) {
        up(&cvp->sem); // 唤醒一个正在等待者
        monitor_yield(mtp); // 让出管程执行权
    }
    cprintf("cond_signal end: cvp %x, cvp->count %d, mtp->next_count %d\n", cvp, cvp->count, mtp->next_count);
}

// Suspend calling thread on a condition variable waiting for condition Atomically unlocks
// mutex and suspends calling thread on conditional variable after waking up locks mutex. Notice: mp is mutex semaphore
// for monitor's procedures
void cond_wait(condvar_t *cvp, monitor_t *mtp) {
    // LAB7 EXERCISE1: YOUR CODE
    cprintf("cond_wait begin:  cvp %x, cvp->count %d, mtp->next_count %d\n", cvp, cvp->count, mtp->next_count);
    /*
     *         cv.count ++;
     *         if(mt.next_count>0)
     *            signal(mt.next)
     *         else
     *            signal(mt.mutex);
     *         wait(cv.sem);
     *         cv.count --;
     */
    cvp->count++;
    monitor_unlock(mtp);
    down(&cvp->sem); // 等待条件满足
    cvp->count--; // 这里实现的是 Hoare 管程, 发出 signal 的进程会立即让出管程, 因此被唤醒者立即满足条件, 无需再判断
    cprintf("cond_wait end:  cvp %x, cvp->count %d, mtp->next_count %d\n", cvp, cvp->count, mtp->next_count);
}
