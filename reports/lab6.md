# Lab 6: 调度器

本实验把之前的 `schedule` 函数的逻辑做了解耦，实现了一个进程调度框架，并在此基础上实现了 Round Robin 调度算法和 Stride 调度算法。

## 跑通 Round Robin 算法

首先合并代码，然后对此前编写的部分代码进行修改，主要是创建进程控制块的初始化代码和时钟中断处理部分。运行 `make grade` 可以看到只有 `priority` 程序不通过，其它都没有问题。

## 分析进程调度框架

回顾前面实现进程机制的实验，可以看到与进程调度相关的主要有两个地方，第一是 `schedule` 函数，这个函数在若干个场合下被调用，以执行进程调度逻辑（找到下一个应该运行的进程并切换过去），第二是 `proc->need_resched`，这个变量在若干个场合被设置为 1，以表示当前进程需要被重新调度，除了 yield 系统调用，最主要的就是时钟中断，之前的逻辑是每 `TICK_NUM` 个时钟中断，将其置 1。

本实验引入的进程调度框架将上面的部分逻辑进行了抽象，通过一个就绪进程队列（`run_queue`）管理可被调度的进程，抽象出了入队（`enqueue`）、选择下一个要执行的进程（`pick_next`）、出队（`dequeue`）、时钟 tick（`proc_tick`）这几个基本操作，通过和前面实验相类似的「面向对象」的方式使得调度算法的实现与内核其它部分解耦。于是 `schedule` 函数只需要直接调用进程调度框架提供的 `enqueue`、`dequeue`、`pick_next` 接口，时钟中断中不再修改 `proc->need_resched` 而是调用 `proc_tick` 让调度算法决定是否需要重新调度。

## 分析 Round Robin 算法过程

RR 调度算法的实现非常简单，进程进入就绪队列时，给它一个时间片（`rq->max_time_slice`），每次 tick 递减该时间片，减到 0 即时间片用完，重新调度。

## 多级反馈队列调度算法设计思路

维护一个就绪队列的数组（而不是 RR 的单个就绪队列），数组下标作为队列的优先级，算法各操作大致逻辑如下：

- `enqueue` 时，如果进程是第一次入队，则放到优先级最高的队列中，如果进程不是第一次入队，且上一次运行用完了时间片，则将它放到更低优先级的队列中，根据放入的队列不同，分配的时间片数量也不同
- `dequeue` 时，像 RR 一样将进程出队
- `pick_next` 时，按优先级依次检查各队列，选择第一个可被调度的就绪进程
- `proc_tick` 时，像 RR 一样，递减时间片（不同优先级的队列可采取不同的调度算法，例如最低优先级可采用 FCFS 算法，那么 tick 就不改变任何东西）

TODO：这个看起来不复杂，后面有空可以尝试具体实现一下。

## 实现 Stride 调度算法

根据 Stride 调度算法的论文，该算法的基本思路就是，每个进程定义三个属性：

- `tickets`，用于表示进程的优先级，数值越大优先级越高，分配的执行时间越多
- `stride`，用于表示进程每次执行所消耗的虚拟时间长度，值为 `stride1 / tickets`，其中 `stride1` 是一个全局的较大常量
- `pass`，用于表示进程所消耗的累计虚拟时间长度，每次调度进程，会递增 `stride`，即 `pass += stride`

调度器每次进行调度时，取所有就绪进程中 `pass` 值最小的执行。

回头阅读实验指导书，巩固了这个理解（需要注意的是，实验指导书中将 `stride` 和 `pass` 弄反了），并且了解到 `stride1` 的数值的选择问题，可以证明 `PASS_MAX - PASS_MIN <= STRIDE_MAX`，而 `tickets >= 1`，因此 `PASS_MAX - PASS_MIN <= stride1`，要使无符号数表示的 `pass` 在发生溢出时也能正确比较大小，需要设置 `stride1` 小于等于相同位数的有符号数所能表示的最大正数。

理解了算法过程后，实现非常简单，只需要在 RR 调度算法的基础上添加一些维护 `pass` 值和优先队列的代码即可。

## 参考资料

- [调度框架和调度算法设计与实现 - 实验指导书](https://chyyuu.gitbooks.io/ucore_os_docs/content/lab6/lab6_3_scheduler_design.html)
- [Lottery and Stride Scheduling: Flexible Proportional-Share  Resource Management](http://www.waldspurger.org/carl/papers/phd-mit-tr667.pdf), Section 3.3 Deterministic Stride Scheduling
- [Stride Scheduling](https://cs.nyu.edu/rgrimm/teaching/sp08-os/stride.pdf)
