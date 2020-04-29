#include <defs.h>
#include <sem.h>
#include <sfs.h>

/*
 * lock_sfs_fs - lock the process of  SFS Filesystem Rd/Wr Disk Block
 *
 * called by: sfs_load_inode, sfs_sync, sfs_reclaim
 */
void lock_sfs_fs(struct sfs_fs *sfs) {
    sem_wait(&(sfs->fs_sem));
}

/*
 * lock_sfs_io - lock the process of SFS File Rd/Wr Disk Block
 *
 * called by: sfs_rwblock, sfs_clear_block, sfs_sync_super
 */
void lock_sfs_io(struct sfs_fs *sfs) {
    sem_wait(&(sfs->io_sem));
}

/*
 * unlock_sfs_fs - unlock the process of  SFS Filesystem Rd/Wr Disk Block
 *
 * called by: sfs_load_inode, sfs_sync, sfs_reclaim
 */
void unlock_sfs_fs(struct sfs_fs *sfs) {
    sem_signal(&(sfs->fs_sem));
}

/*
 * unlock_sfs_io - unlock the process of sfs Rd/Wr Disk Block
 *
 * called by: sfs_rwblock sfs_clear_block sfs_sync_super
 */
void unlock_sfs_io(struct sfs_fs *sfs) {
    sem_signal(&(sfs->io_sem));
}
