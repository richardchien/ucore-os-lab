#ifndef __KERN_SYNC_SEM_H__
#define __KERN_SYNC_SEM_H__

#include <atomic.h>
#include <defs.h>
#include <wait.h>

typedef struct {
    int value;
    wait_queue_t wait_queue;
} semaphore_t;

void sem_init(semaphore_t *sem, int value);
void sem_signal(semaphore_t *sem);
void sem_wait(semaphore_t *sem);
bool sem_try(semaphore_t *sem);

#endif /* !__KERN_SYNC_SEM_H__ */
