# Lab 5: 用户进程管理

本实验实现了基本的用户进程管理,主要包括用户进程的创建和为用户进程提供服务的系统调用。

## 合并实验代码

这一步本不应该出问题，但在这次实验还是出现了非常奇怪的问题：添加了一些代码后，`check_pgfault` 检查无法通过，在 `free_page` 调用中 assert 失败。调试了很久，排除了很多可能，得出了一些基本判断：

1. 问题与本次实验内容无关，因为删掉一开始添加的代码，改为无意义的 print，仍然出现同样问题
2. 问题可能不是因为前面实验的代码有错误，因为删掉添加的代码后问题不再存在
3. 问题发生的直接原因是 `check_pgfault` 中的 for 循环处，访问虚拟地址 `0x100` 附近的内容时，本应触发缺页异常，但却没有触发，进而导致页表没有被创建，随后 `free_page` 试图释放一个错误的内存页
4. 问题可能与 GCC 或 QEMU 的某个参数的默认值有关，有一个明显的临界点，多一行代码就出问题，少一行就不出问题

后来在群里讨论了一下，两位群友在他们的环境下运行我的代码都没有出问题，从而证实问题出在我的环境上，一位群友提出可能跟 QEMU 的 TLB 有关，页表项被缓存了，于是尝试在 `check_pgfault` 中加入 `tlb_invalidate(pgdir, addr)` 来刷新 TLB，果然解决了问题。

总结来看这个问题应该可以算作 uCore 的一个小 bug 了。

## 完成实验

本实验需要填写的代码较为简单，根据实验指导书和代码注释的提示可以直接写出，写完后第一次运行 `make grade` 时发现 `spin`、`forktree` 等测试程序结果错误，检查报错发现在 `trap_dispatch` 中调用 `print_ticks` 时，会直接 panic，因此在时钟中断中，应该删掉前面实验写的 `print_ticks` 调用，只写 `current->need_resched = 1` 即可。

## 分析用户进程从创建到执行的过程

从 `proc_init` 开始，首先硬创建了 idle 内核线程（pid 为 0），然后使用 `kernel_thread` 函数创建了 init 内核线程（pid 为 1），入口函数为 `init_main`，这个函数又创建了一个内核线程（pid 为 2），执行 `user_main` 函数。`user_main` 函数会根据宏定义的情况，选择加载某个用户程序来执行，默认情况下会加载 `exit` 程序，一系列宏展开后，实际上调用了 `kernel_execve` 函数，这个函数直接使用 `int` 指令进行了一个系统调用。

触发系统调用后，程序进入中断处理流程，执行到 `trap_dispatch` 函数，进而调用 `syscall` 函数，该函数从 trapframe 里面读出系统调用编号和参数，然后从 `syscalls` 函数指针数组中取出对应的系统调用函数执行，对于这里的情况，会执行 `sys_exec` 函数，进而调用 `do_execve` 函数。`do_execve` 函数首先清除了当前进程的内存映射，然后调用 `load_icode` 加载内核二进制文件中的用户程序代码，后者依次从给定的地址加载 ELF 文件、分配内存、建立内存映射、拷贝程序的各段、分配用户栈等，最后修改 trapframe 中的一些寄存器值，使待会儿中断返回时能够跳转到刚刚加载的程序执行。

最后中断返回，程序跳转到 `load_icode` 函数中所设置的 `tf->tf_eip` 处，也即 `elf->e_entry`。查看 `tools/user.ld` 可以发现 ELF 的入口被设置为了 `_start` 函数，在 `user/libs/initcode.S` 中定义，这个函数又调用了 `umain` 函数，进而调用了用户程序的 `main` 函数。至此用户进程成功运行。

## COW 机制实现思路

目前 `do_fork` 函数里调用 `copy_mm` 函数完成了进程虚拟地址空间的拷贝，其中调用 `dup_mmap` 函数完成了地址空间映射的复制，该函数复制 vma 链表后，又通过 `copy_range` 函数拷贝了物理内存页。如果要实现 COW，这里不应该拷贝物理内存页，而是将已分配的物理页都设置为只读，然后在 `do_pgfault` 函数中处理缺页异常时，对 `*ptep & PTE_P == 1` 的情况（要写入的页是只读的）做特别处理，分配一个新的物理页，设置为可写，并拷贝物理页内容。

## 分析 fork、exec、wait、exit 如何影响进程的执行状态

### fork（`do_fork` 函数）

首先调用 `alloc_proc` 函数分配进程控制块，此时进程状态初始化为 `PROC_UNINIT`，一系列设置完成后，调用 `wakeup_proc` 函数将进程状态修改为 `PROC_RUNNABLE`，从而可被调度。

### exec（`do_execve` 函数）

没有改变进程的状态，这是符合预期的，毕竟进程只有是 `PROC_RUNNABLE` 状态才可能执行到这个函数，而这个函数运行后，新的程序代码也应当立即可以被调度。

### wait（`do_wait` 函数）

这个函数根据 `pid` 参数寻找需要等待的进程，有多种情形：

1. 如果找到 `PROC_ZOMBIE` 状态的进程，则回收该进程并返回
2. 如果 `pid` 所指定的进程或子进程存在，但都不在 `PROC_ZOMBIE` 状态，则将当前进程设置为 `PROC_SLEEPING` 状态，并重新调度进程
3. 如果 `pid` 所指定的进程或子进程不存在，则返回一个错误

### exit（`do_exit` 函数）

首先将当前进程修改为 `PROC_ZOMBIE` 状态，并唤醒正在等待的父进程到 `PROC_RUNNABLE` 状态。如果当前进程有子进程，则由 init 内核线程接管这些子进程。

## 参考资料

- [用户进程管理 - 实验指导书](https://chyyuu.gitbooks.io/ucore_os_docs/content/lab5/lab5_3_user_process.html)
- [用户进程的特征 - 实验指导书](https://chyyuu.gitbooks.io/ucore_os_docs/content/lab5/lab5_5_appendix.html)
