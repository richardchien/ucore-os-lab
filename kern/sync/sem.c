#include <assert.h>
#include <atomic.h>
#include <defs.h>
#include <kmalloc.h>
#include <proc.h>
#include <sem.h>
#include <sync.h>
#include <wait.h>

void sem_init(semaphore_t *sem, int value) {
    sem->value = value;
    wait_queue_init(&(sem->wait_queue));
}

static __noinline void __signal(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        wait_t *wait;
        if ((wait = wait_queue_first(&(sem->wait_queue))) == NULL) {
            sem->value++;
        } else {
            assert(wait->proc->wait_state == wait_state);
            wakeup_wait(&(sem->wait_queue), wait, wait_state, 1);
        }
    }
    local_intr_restore(intr_flag);
}

static __noinline uint32_t __wait(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    local_intr_save(intr_flag);
    if (sem->value > 0) { // 如果还有资源
        sem->value--; // 则获取成功
        local_intr_restore(intr_flag);
        return 0;
    }
    wait_t __wait, *wait = &__wait;
    wait_current_set(&(sem->wait_queue), wait, wait_state); // 否则进入等待队列
    local_intr_restore(intr_flag);

    schedule(); // 并让出 CPU

    // 当 schedule 返回时, 说明有 signal 操作唤醒了本进程, 并把资源使用权转让给本进程,
    // 由于 signal 时 sem->value 并没有增加, 因此资源不会被其它进程抢占
    local_intr_save(intr_flag);
    wait_current_del(&(sem->wait_queue), wait);
    local_intr_restore(intr_flag);

    if (wait->wakeup_flags != wait_state) {
        return wait->wakeup_flags;
    }
    return 0;
}

void sem_signal(semaphore_t *sem) {
    __signal(sem, WT_KSEM);
}

void sem_wait(semaphore_t *sem) {
    uint32_t flags = __wait(sem, WT_KSEM);
    assert(flags == 0);
}

bool sem_try(semaphore_t *sem) {
    bool intr_flag, ret = 0;
    local_intr_save(intr_flag);
    if (sem->value > 0) {
        sem->value--, ret = 1;
    }
    local_intr_restore(intr_flag);
    return ret;
}
