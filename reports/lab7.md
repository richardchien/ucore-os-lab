# Lab 7: 同步互斥

本实验主要涉及等待队列、定时器、信号量、管程的实现。

## 理解内核级信号量的实现

信号量的标准实现一般是这样：

```c
typedef struct semaphore {
    int value;
    wait_queue_t queue;
} semaphore_t;

void wait(semaphore_t *sem) {
    sem->value--;
    if (sem->value < 0) {
        // 进入等待队列 sem->queue 并阻塞
    }
}

void signal(semaphore_t *sem) {
    sem->value++;
    if (sem->value <= 0) {
        // 唤醒等待队列中的一个进程
    }
}
```

本实验中给出的 `kern/sync/sem.c` 基本结构跟这个一样，不过细节上有一处不同：在 `wait`（即 `down`）时，只有 `sem->value > 0` 才会减 1，否则直接进入等待，而 `signal`（即 `up`）时，只有当前等待队列为空时才加 1，否则直接唤醒等待中的进程。从效果上看这和上面的标准实现没有本质区别。

## 为用户态进程提供信号量机制的设计思路

可以通过系统调用提供创建信号量（`sem = create_semaphore(name)`）、打开已有信号量（`sem = open_semaphore(name)`）、获取资源（`wait_semaphore(sem)`）、释放资源（`signal_semaphore(sem)`）等接口，来向用户态进程提供信号量机制。在实现细节上，与内核级信号量的区别在于 PV 操作不需要通过屏蔽中断来保证原子性，因为 uCore 内核本身就是不可抢占的，当内核在处理系统调用时，不可能有其它用户进程获得 CPU 使用权。

## 理解内核级条件变量的实现

本实验中实现的管程机制是 Hoare 管程，也就是说，当一个进程释放资源后使得先前进入等待的进程条件满足时，通过 `signal` 操作将直接把管程让给后者，而自身进入等待，后者随即获得资源，从 `wait` 返回，并离开管程，离开时再次唤醒刚刚因 `signal` 而等待的进程。

首先看 `kern/sync/check_sync.c` 中哲学家就餐问题所使用的管程操作 `phi_take_forks_condvar`、`phi_put_forks_condvar`，这两个函数都是互斥进入的，同一时刻只有一个进程在里面执行，其它进程要么在 `mtp->mutex` 处等待，要么在 `mtp->next` 处等待。`phi_take_forks_condvar` 中，当一个进程获取 `mtp->mutex` 后，发现条件不满足时，它会调用 `cond_wait` 来等待条件变量 `mtp->cv[i]`，而 `cond_wait` 函数中释放了 `mtp->mutex`（如果当前有在 `mtp->next` 处等待的进程，则直接让它执行），然后等待条件变量中的信号量。`phi_put_forks_condvar` 中，当一个进程使用完资源后，会检查是否有其它进程的条件可以满足，一旦满足，就通过 `cond_signal` 唤醒它，由于唤醒它使它立即获得了管程使用权，因此当前进程需要在 `mtp->next` 上等待，直到被唤醒的进程离开管程。

**Update:** 由于 uCore 中原本的管程和条件变量实现完全互相依赖，因此后来对这部分进行了解耦，管程只用来控制对管程过程互斥性的访问，其中主要维护入口等待队列（等在 `mtp->mutex`）和 signal 等待队列（等在 `mtp->next`），而条件变量用来控制对条件的等待和通知，两者互不访问对方的结构体内容，而只是通过函数接口来交互。解耦之后，哲学家就餐的问题实现就变得清楚很多了。

## 参考资料

- [同步互斥机制的设计与实现 - 实验指导书](https://chyyuu.gitbooks.io/ucore_os_docs/content/lab7/lab7_3_synchronization_implement.html)
- [哲学家就餐问题 - 维基百科](https://zh.wikipedia.org/wiki/%E5%93%B2%E5%AD%A6%E5%AE%B6%E5%B0%B1%E9%A4%90%E9%97%AE%E9%A2%98)
