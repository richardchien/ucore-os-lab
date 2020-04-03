# Lab 4: 内核线程管理

本实验实现了基本的内核线程管理，主要包括内核线程的创建和调度过程。

首先阅读实验指导书了解了实验的背景知识和新增的代码分析，已经非常详尽了，这里不再罗列。

## 理解执行流的切换过程

个人觉得需要费一些力气理解的点是 `struct proc_struct` 中 `context` 和 `tf` 字段中各自保存的寄存器信息的区别。

`proc->context` 的数据是直接保存在 `struct proc_struct` 结构体的内存中的，在 `switch_to` 函数中进行寄存器的保存和恢复，将 `next->context` 的内容恢复后，通过 `ret` 指令返回，实际上跳转到 `next->context.eip` 所指的地址，对于「第 1 个」内核线程 `init`，该地址是由 `do_fork` 调用 `copy_thread` 设置的，值为 `forkret` 函数的地址（在 `kern/trap/trapentry.S` 中定义）；而 `proc->tf` 是一个指针，指向 `proc->kstack` 所指向的内核栈中的一个 `struct trapframe`（对于 `init` 内核线程，这是人为构造出来的），前面的 `forkret` 函数执行时，修改了 ESP，把 `next->tf` 所指的地址设置成了当前栈顶，然后通过 `iret` 指令进行中断返回，巧妙地利用硬件在中断返回时的一系列操作跳转到真正的内核线程入口 `kernel_thread_entry`。

这个过程乍看起来有些多此一举，先用 `ret` 跳转了一次，又用 `iret` 跳转了一次，但实际上是必要的，因为在进程真正运行的时候，通常是通过中断触发进程调度，此时被中断的进程的状态是保存在 `struct trapframe` 中的，这是 CPU 和 `kern/trap/trapentry.S` 的 `__alltraps` 共同完成的，并且这个过程中可能存在特权级的切换，因此要重新继续运行这个进程，应当使用 `iret` 中断返回指令。

另一方面，从 `idle` 切换到 `init` 内核线程时，`prev->context`（也即 `idleproc->context`）中保存的 EIP 是 `proc_run` 中调用 `switch_to` 语句之后的指令地址，顺着调用栈可以发现，当再次调度 `idle` 时，它将最终返回到 `cpu_idle` 函数中继续循环。而当真实的许多进程运行时，假设在某个中断内进行了进程调度，这时的 EIP 被保存到 `proc->context`，那么当它下次被调度时，`switch_to` 会使它先返回到中断处理程序，然后通过 `iret` 返回到进程的实际代码，这和前面所描述的创建内核线程时在栈底人为构造一个 `struct trapframe` 并利用 `iret` 跳转到 `kernel_thread_entry` 的过程实际上是一样的。

由于需要精细操控栈上的 `struct trapframe`，并且需要做寄存器状态的保存和恢复，因此在 `proc_run` 函数的首尾通过 `local_intr_save` 和 `local_intr_restore` 分别关闭和恢复中断，以保证进程切换过程的原子性。

## 实现 `alloc_proc` 和 `do_fork`

虽然分为了两个练习，但无法割裂地实现这两个函数，在实际运行中，首先通过 `alloc_proc` 分配一个初始化的进程控制块，然后 `do_fork` 以当前进程和相关参数提供的数据对新进程控制块进行修改和配置。

需要自行编写的代码在实验指导书和注释中都给出了提示，填写如下：

```c
// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = (uintptr_t)bootstack;
        proc->need_resched = 0;
        proc->parent = current;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(proc->context));
        proc->tf = NULL;
        proc->cr3 = boot_cr3;
        proc->flags = 0;
        set_proc_name(proc, "");
    }
    return proc;
}

/* do_fork -     parent process for a new child process
 * @clone_flags: used to guide how to clone the child process
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;

    if (!(proc = alloc_proc())) goto fork_out;
    if (setup_kstack(proc) != 0) goto bad_fork_cleanup_proc;
    if (copy_mm(clone_flags, proc) != 0) goto bad_fork_cleanup_kstack;
    copy_thread(proc, stack, tf);
    proc->pid = nr_process++;
    hash_proc(proc);
    list_add(&proc_list, &(proc->list_link));
    proc->state = PROC_RUNNABLE;
    ret = proc->pid;

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

## 修正上述实现存在的问题

看到练习里问了「能否做到给每个新 fork 的线程一个唯一的 id」，觉得有点奇怪，想了一会儿觉得应该是可以，然而看了参考答案后恍然大悟，由于没有禁止中断处理，设置 `proc->pid` 那块的操作可能不是原子的（而且上面的实现没充分利用已经给出的 `get_pid` 和 `wakeup_proc` 函数），因此是有可能出现一个进程分配了 pid 后立即中断，然后中断时创建了新进程获得了同样的 pid 的情况的。

参照答案修改如下：

```c
// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(proc->context));
        proc->tf = NULL;
        proc->cr3 = boot_cr3;
        proc->flags = 0;
        set_proc_name(proc, "");
    }
    return proc;
}

/* do_fork -     parent process for a new child process
 * @clone_flags: used to guide how to clone the child process
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;

    if (!(proc = alloc_proc())) goto fork_out;
    proc->parent = current;
    if (setup_kstack(proc) != 0) goto bad_fork_cleanup_proc;
    if (copy_mm(clone_flags, proc) != 0) goto bad_fork_cleanup_kstack;
    copy_thread(proc, stack, tf);

    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        list_add(&proc_list, &(proc->list_link));
        nr_process++;
    }
    local_intr_restore(intr_flag);

    wakeup_proc(proc);
    ret = proc->pid;

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

## 参考资料

- [内核线程管理 - 实验指导书](https://chyyuu.gitbooks.io/ucore_os_docs/content/lab4/lab4_3_kernel_thread_management.html)
