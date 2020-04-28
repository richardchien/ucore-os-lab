#include <assert.h>
#include <kmalloc.h>
#include <monitor.h>
#include <stdio.h>

// Initialize monitor.
void monitor_init(monitor_t *mtp, size_t num_cv) {
    int i;
    assert(num_cv > 0);
    mtp->next_count = 0;
    mtp->cv = NULL;
    sem_init(&(mtp->mutex), 1); // unlocked
    sem_init(&(mtp->next), 0);
    mtp->cv = (condvar_t *)kmalloc(sizeof(condvar_t) * num_cv);
    assert(mtp->cv != NULL);
    for (i = 0; i < num_cv; i++) {
        mtp->cv[i].count = 0;
        sem_init(&(mtp->cv[i].sem), 0);
        mtp->cv[i].owner = mtp;
    }
}

// Unlock one of threads waiting on the condition variable.
void cond_signal(condvar_t *cvp) {
    // LAB7 EXERCISE1: YOUR CODE
    cprintf("cond_signal begin: cvp %x, cvp->count %d, cvp->owner->next_count %d\n",
            cvp,
            cvp->count,
            cvp->owner->next_count);
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
        cvp->owner->next_count++;
        up(&cvp->sem); // 唤醒一个正在等待者
        down(&cvp->owner->next); // 等待被唤醒者离开管程
        cvp->owner->next_count--;
    }
    cprintf(
        "cond_signal end: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}

// Suspend calling thread on a condition variable waiting for condition Atomically unlocks
// mutex and suspends calling thread on conditional variable after waking up locks mutex. Notice: mp is mutex semaphore
// for monitor's procedures
void cond_wait(condvar_t *cvp) {
    // LAB7 EXERCISE1: YOUR CODE
    cprintf("cond_wait begin:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n",
            cvp,
            cvp->count,
            cvp->owner->next_count);
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
    if (cvp->owner->next_count > 0) {
        up(&cvp->owner->next); // 唤醒由于调用 cond_signal 而等待的进程
    } else {
        up(&cvp->owner->mutex); // 唤醒由于互斥锁而等待进入管程的进程
    }
    down(&cvp->sem); // 等待条件满足
    cvp->count--; // 这里实现的是 Hoare 管程, 发出 signal 的进程会立即让出管程, 因此被唤醒者立即满足条件, 无需再判断
    cprintf(
        "cond_wait end:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}
