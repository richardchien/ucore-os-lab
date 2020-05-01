# Lab 8: 文件系统

本实验在 uCore 中实现了简单的文件系统支持，加载并运行程序的方式从之前的直接读取内核二进制中附加的用户程序改为了从文件系统读取。

## 理解文件系统的实现

首先合并了实验代码，发现新增了很多内容，对照实验指导书消化了很久，逐渐理解了文件系统的设计逻辑。

最顶层抽象是通用文件系统访问接口，实际上就是暴露给用户的系统调用接口，包括 `read`、`write`、`open`、`close` 等耳熟能详的操作，这一层所处理的参数是文件名、文件描述符、读写 buffer 等，不会接触到文件的抽象表示 `struct inode`，更不会接触到具体的 SimpleFS 文件系统中的 `struct sfs_inode` 和写入到磁盘上的数据结构。这些系统调用内部直接调用 `sysfile_open`、`sysfile_read` 等名为 `sysfile_*` 的函数，这些函数利用低一层的接口来实现功能，并在内核态和用户态之间传送数据。

`sysfile_*` 等函数调用 `file_*` 等函数来实现具体功能，后者维护着进程的打开文件表（`proc->filesp`），而打开文件表里面存的是 `struct file` 表示的文件，因此 `file_*` 等函数内部降低了一层抽象，在文件描述符和 `struct file` 之间转换，而 `struct file` 中保存着文件的 `struct inode` 和当前读写位置等，在向下一层调用 `vop_*` 等函数时，将使用 `file->node` 等作为参数。`file_open` 和 `file_close` 稍微特殊些，它们调用的是 `vfs_open` 和 `vfs_close`。

`vfs_*` 和 `vop_*` 这一层称作文件系统抽象层（VFS），管理底层各种设备和文件系统，提供统一的一组接口和数据结构，通过在结构体中存放函数指针来向上层屏蔽底层的具体实现。

TODO：VFS 这一部分理解的还不够清晰，也许需要查看更多其它 kernel 的实现。

`vop_*` 等函数实际上是一组宏，它们最终调用的是 `struct inode` 中保存的 `in_ops` 中的函数指针，而这些指针实际指向的是 SimpleFS 的 `sfs_openfile`、`sfs_close`、`sfs_read` 或设备的 `dev_read`、`dev_write` 等函数，这时就会有各不相同的操作逻辑。

## 补全缺少的代码

练习 1 要求补全 `sfs_io_nolock` 函数，练习 2 要求 `load_icode` 函数，这两者都是从磁盘加载用户程序运行的必经函数，所以必须一次写完才能跑通。

`sfs_io_nolock` 函数较为简单，主要就是利用 `sfs_bmap_load_nolock` 获得文件中的第某块对应的磁盘块编号，然后再使用 `sfs_buf_op` 或 `sfs_block_op` 读写，前者用于读写一块内的若干长度数据，后者用于读写多干个完整的块。所以 `sfs_io_nolock` 要补充的内容分为三部分：首先读写没有对齐到块的部分，然后读写中间的整块数据，最后读写尾部没有对齐到块的部分。

`load_icode` 有点复杂，步骤很多，不过大多数步骤可以直接参考前面几次实验中的实现，不涉及加载用户程序的部分，基本完全一致，例如设置 mm、页目录、添加内存映射等，加载 ELF 文件的部分需要稍作修改，把从 `binary` 地址处拷贝改为从文件读取。设置 `argc` 和 `argv` 这部分一开始卡住了，主要问题在于不确定 `argv` 的字符串应该放在哪，后来去参考了答案，发现是直接拷贝到用户栈底部，实现起来比较方便，最终成功完成。

## PIPE 机制的设计思路

TODO

## 硬链接和软链接机制的设计思路

硬链接可以通过在 `struct sfs_disk_entry` 中保存相同的 `ino` 来实现，也就是在目录中查找时，不同的文件名最终指向了同一个 `struct sfs_disk_inode`，同时维护它的 `nlinks`，当该值降为 0 时可删除文件。

软链接可以通过在文件的数据块中保存它所链接到的路径来实现，同时为了能够辨别普通文件和软链接，需要设置 `struct sfs_disk_inode` 的 `type` 为 `SFS_TYPE_LINK`。

## 参考资料

- [文件系统设计与实现 - 实验指导书](https://chyyuu.gitbooks.io/ucore_os_docs/content/lab8/lab8_3_fs_design_implement.html)
- [设备文件 IO 层 - 实验指导书](https://chyyuu.gitbooks.io/ucore_os_docs/content/lab8/lab8_3_5_dev_file_io_layer.html)
- [文件操作实现 - 实验指导书](https://chyyuu.gitbooks.io/ucore_os_docs/content/lab8/lab8_3_7_file_op_implement.html)
