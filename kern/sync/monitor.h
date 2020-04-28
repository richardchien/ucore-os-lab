#ifndef __KERN_SYNC_MONITOR_CONDVAR_H__
#define __KERN_SYNC_MOINTOR_CONDVAR_H__

#include <sem.h>
/* In [OS CONCEPT] 7.7 section, the accurate define and approximate implementation of MONITOR was introduced.
 * INTRODUCTION:
 *  Monitors were invented by C. A. R. Hoare and Per Brinch Hansen, and were first implemented in Brinch Hansen's
 *  Concurrent Pascal language. Generally, a monitor is a language construct and the compiler usually enforces mutual
 * exclusion. Compare this with semaphores, which are usually an OS construct. DEFNIE & CHARACTERISTIC: A monitor is a
 * collection of procedures, variables, and data structures grouped together. Processes can call the monitor procedures
 * but cannot access the internal data structures. Only one process at a time may be be active in a monitor. Condition
 * variables allow for blocking and unblocking. cv.wait() blocks a process. The process is said to be waiting for (or
 * waiting on) the condition variable cv. cv.signal() (also called cv.notify) unblocks a process waiting for the
 * condition variable cv. When this occurs, we need to still require that only one process is active in the monitor.
 * This can be done in several ways: on some systems the old process (the one executing the signal) leaves the monitor
 * and the new one enters on some systems the signal must be the last statement executed inside the monitor. on some
 * systems the old process will block until the monitor is available again. on some systems the new process (the one
 * unblocked by the signal) will remain blocked until the monitor is available again. If a condition variable is
 * signaled with nobody waiting, the signal is lost. Compare this with semaphores, in which a signal will allow a
 * process that executes a wait in the future to no block. You should not think of a condition variable as a variable in
 * the traditional sense. It does not have a value. Think of it as an object in the OOP sense. It has two methods, wait
 * and signal that manipulate the calling process. IMPLEMENTATION: monitor mt {
 *     ----------------variable------------------
 *     semaphore mutex;
 *     semaphore next;
 *     int next_count;
 *     condvar {int count, sempahore sem}  cv[N];
 *     other variables in mt;
 *     --------condvar wait/signal---------------
 *     cond_wait (cv) {
 *         cv.count ++;
 *         if(mt.next_count>0)
 *            signal(mt.next)
 *         else
 *            signal(mt.mutex);
 *         wait(cv.sem);
 *         cv.count --;
 *      }
 *
 *      cond_signal(cv) {
 *          if(cv.count>0) {
 *             mt.next_count ++;
 *             signal(cv.sem);
 *             wait(mt.next);
 *             mt.next_count--;
 *          }
 *       }
 *     --------routines in monitor---------------
 *     routineA_in_mt () {
 *        wait(mt.mutex);
 *        ...
 *        real body of routineA
 *        ...
 *        if(next_count>0)
 *            signal(mt.next);
 *        else
 *            signal(mt.mutex);
 *     }
 */

typedef struct monitor {
    semaphore_t mutex; // 用于实现互斥地进入管程
    semaphore_t next; // 用于维护中途让出管程(唤醒了其它进程)的等待进程队列
    int next_count; // 等在 next 队列中的进程数
} monitor_t;

// Initialize variables in monitor.
void monitor_init(monitor_t *mtp);
// Lock monitor procedure.
void monitor_lock(monitor_t *mtp);
// Unlock monitor procedure.
void monitor_unlock(monitor_t *mtp);
// Yield the ownership of monitor, to let the newly waked-up process run.
void monitor_yield(monitor_t *mtp);

typedef struct condvar {
    semaphore_t sem; // 利用信号量实现等待和通知
    int count; // 正在等待此条件变量的进程数
} condvar_t;

// Initialize condition variable.
void cond_init(condvar_t *cvp);
// Unlock one of threads waiting on the condition variable.
void cond_signal(condvar_t *cvp, monitor_t *mtp);
// Suspend calling thread on a condition variable waiting for condition atomically unlock mutex in monitor,
// and suspends calling thread on conditional variable after waking up locks mutex.
void cond_wait(condvar_t *cvp, monitor_t *mtp);

#endif /* !__KERN_SYNC_MONITOR_CONDVAR_H__ */
