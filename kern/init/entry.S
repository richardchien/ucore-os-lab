#include <mmu.h>
#include <memlayout.h>

#define REALLOC(x) (x - KERNBASE)

.text
.globl kern_entry
kern_entry:  # 虚拟地址 0xC0100000, 物理地址 0x00100000
    # load pa of boot pgdir
    movl $REALLOC(__boot_pgdir), %eax  # 把 boot 期间所用的页目录的物理地址加载到 CR3
    movl %eax, %cr3

    # enable paging
    # 注意这里启用页机制的逻辑已经和实验指导书 3.3.5.3 不一样了,
    # 不再需要 enable_paging 函数, 也不需要临时的 -0xC0000000 段.
    # kern_entry 以物理地址 0x00100000 (此时线性地址即物理地址) 被调用,
    # 在 movl %eax, %cr0 之前, CPU 能够通过增加 PC 的值来直接以物理地址找到下一条指令,
    # 而 movl %eax, %cr0 之后, 临时页目录项 0 ~ 4M VA -> 0 ~ 4M PA 使 CPU 也能正确找到指令
    movl %cr0, %eax
    orl $(CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP), %eax
    andl $~(CR0_TS | CR0_EM), %eax
    movl %eax, %cr0  # 正式启用页机制, 这之后 CPU 将把线性地址看做页式内存虚拟地址, 去页表中查找物理地址

    # update eip
    # 此时 EIP 仍然是 0x0010????
    leal next, %eax  # 这里的 next 是 0xC010???? (obj/kernel.asm 中可见)
    # set eip = KERNBASE (0xC0000000) + 0x10????
    jmp *%eax  # 于是这里跳转指令将 EIP 设置为了虚拟地址
next:

    # unmap va 0 ~ 4M, it's temporary mapping
    xorl %eax, %eax
    movl %eax, __boot_pgdir

    # set ebp, esp
    movl $0x0, %ebp
    # the kernel stack region is from bootstack -- bootstacktop,
    # the kernel stack size is KSTACKSIZE (8KB) defined in memlayout.h
    movl $bootstacktop, %esp
    # now kernel stack is ready , call the first C function
    call kern_init

# should never get here
spin:
    jmp spin

.data
.align PGSIZE  # 按页大小对齐, 使得内核栈正好占用完整的两页
    .globl bootstack
bootstack:
    .space KSTACKSIZE  # 这里填充了 KSTACKSIZE (8KB) 的空间作为内核栈
    .globl bootstacktop
bootstacktop:  # 内核栈的最高地址 (逻辑上是栈底)

# kernel builtin pgdir
# an initial page directory (Page Directory Table, PDT)
# These page directory table and page table can be reused!
# PDT 共有 1024 项, 每项 4 字节, 总计正好 4K, 一页.
# 每个 PDE 负责映射 4MB 内存, 也就是每个 PDE 会指向一个 1024 项的 PT,
# 而每个 PTE 又是 4 字节, 于是每个 PT 也正好一页.
.section .data.pgdir
.align PGSIZE
__boot_pgdir:
.globl __boot_pgdir
    # map va 0 ~ 4M to pa 0 ~ 4M (temporary), 临时映射用于在启动页机制后的一小短时间内能够正确执行程序
    .long REALLOC(__boot_pt1) + (PTE_P | PTE_U | PTE_W)
    # KERNBASE 内核起始虚拟地址, 右移 PGSHIFT (12) 位得到页号, 再右移 10 位得到页目录号, 每个页目录项 (PDE) 占 4 字节, 于是再左移 2 位
    .space (KERNBASE >> PGSHIFT >> 10 << 2) - (. - __boot_pgdir) # pad to PDE of KERNBASE
    # KERNBASE 所对应的页目录项开始
    # map va KERNBASE + (0 ~ 4M) to pa 0 ~ 4M
    .long REALLOC(__boot_pt1) + (PTE_P | PTE_U | PTE_W)
    .space PGSIZE - (. - __boot_pgdir) # pad to PGSIZE (4K), 页目录号有 10 位, 每个 PDE 占 4 字节, 正好一共占满一个页, 这里将剩余部分填充完

.set i, 0
__boot_pt1:  # boot 期间使用的页表, 实际上直接将页号 * PGSIZE (也就是 << PGSHIFT), 映射到物理地址 0 ~ 4MB
.rept 1024
    .long i * PGSIZE + (PTE_P | PTE_W)
    .set i, i + 1
.endr
