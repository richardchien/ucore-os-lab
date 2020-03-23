#include <asm.h>

# Start the CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.set PROT_MODE_CSEG,        0x8                     # kernel code segment selector
.set PROT_MODE_DSEG,        0x10                    # kernel data segment selector
.set CR0_PE_ON,             0x1                     # protected mode enable flag
.set SMAP,                  0x534d4150

# start address should be 0:7c00, in real mode, the beginning address of the running bootloader
.globl start
start:
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment

    # Enable A20:
    #  For backwards compatibility with the earliest PCs, physical
    #  address line 20 is tied low, so that addresses higher than
    #  1MB wrap around to zero by default. This code undoes this.

    # read
    call wait_8042
    movb $0xd0, %al
    outb %al, $0x64
    call wait_8042
    inb $0x60, %al
    # modify
    movb %al, %dl
    orb $0x2, %dl                                   # enable the A20 bit
    # write
    call wait_8042
    movb $0xd1, %al
    outb %al, $0x64
    call wait_8042
    movb %dl, %al
    outb %al, $0x60

probe_memory:
    movl $0, 0x8000            # 0x8000 处开始存放 struct e820map, 前 4 字节是系统内存映射地址描述符（ARD）个数
    xorl %ebx, %ebx            # EBX 清零
    movw $0x8004, %di          # 0x8004 开始存放 struct e820map 的 map 字段, 即以 20 字节为单位的 ARD 数组, BIOS 通过 ES:DI 获得该位置
start_probe:
    movl $0xE820, %eax         # 调用 int 0x15 0xe820 请求 BIOS 进行内存探测
    movl $20, %ecx             # 保存 ARD 的内存大小, 20 字节
    movl $SMAP, %edx           # "SWAP" 字符串
    int $0x15                  # 调用 BIOS 0x15 号中断
    jnc continue_probe         # 如果 int 0x15 执行成功, CF 标志位是 0, 否则是 1
    movw $12345, 0x8000        # CF == 1, 探测失败
    jmp finish_probe
continue_probe:                # CF == 0, 继续探测
    addw $20, %di              # 指出下一个 ARD 应该存放的位置
    incl 0x8000                # 增加 struct e820map 的 nr_map 字段值（增加 ARD 个数）
    cmpl $0, %ebx              # EBX == 0 表示探测已结束
    jnz start_probe            # EBX != 0 则继续探测
finish_probe:

    # Switch from real to protected mode, using a bootstrap GDT
    # and segment translation that makes virtual addresses
    # identical to physical addresses, so that the
    # effective memory map does not change during the switch.
    lgdt gdtdesc
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0

    # Jump to next instruction, but in 32-bit code segment.
    # Switches processor into 32-bit mode.
    ljmp $PROT_MODE_CSEG, $protcseg

wait_8042:
    inb $0x64, %al                                  # Wait for 8042 not busy.
    testb $0x2, %al
    jnz wait_8042
    ret

.code32                                             # Assemble for 32-bit mode
protcseg:
    # Set up the protected-mode data segment registers
    movw $PROT_MODE_DSEG, %ax                       # Our data segment selector
    movw %ax, %ds                                   # -> DS: Data Segment
    movw %ax, %es                                   # -> ES: Extra Segment
    movw %ax, %fs                                   # -> FS
    movw %ax, %gs                                   # -> GS
    movw %ax, %ss                                   # -> SS: Stack Segment

    # Set up the stack pointer and call into C. The stack region is from 0--start(0x7c00)
    movl $0x0, %ebp
    movl $start, %esp
    call bootmain

    # If bootmain returns (it shouldn't), loop.
spin:
    jmp spin

.data
# Bootstrap GDT
.p2align 2                                    # force 4 byte alignment
gdt:
    SEG_NULLASM                               # null seg， selector: 0x0
    SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)     # code seg for bootloader and kernel, selector: 0x8
    SEG_ASM(STA_W, 0x0, 0xffffffff)           # data seg for bootloader and kernel, selector: 0x10

gdtdesc:
    .word 0x17                                      # sizeof(gdt) - 1 (8*3-1 = 23 = 0x17)
    .long gdt                                       # address gdt
