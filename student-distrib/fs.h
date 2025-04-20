/* fs.h -- definitions for the filesystem driver */

#ifndef _FS_H
#define _FS_H

#include "fd.h"

#ifndef ASM

/* 4KiB block size */
#define FS_BLOCK_SIZE 4096
/* max number of directory entries in the boot block */
/* 4KiB block divided by 64B dentry size minus 1 (for boot block header) */
#define FS_MAX_DENTRIES 63
/* max number of data block indices in an inode */
/* 4KiB block divided by 4B block index minus 1 (for length field) */
#define FS_MAX_DBLKS 1023
/* from counting the bytes in the spec */
#define FS_DENTRY_SIZE 64
/* as per spec, filenames can only be at most 32 characters */
#define FS_MAX_FNAME_LEN 32
/* 4KiB == 1 << 12, so the least significant 12 bits of an offset index
 * into a data block (just like pages in x86) */
#define FS_DATA_BLK_BITS 12

#define FS_DENTRY_RTC 0
#define FS_DENTRY_DIR 1
#define FS_DENTRY_FILE 2

typedef struct __attribute__((packed)) dentry_t {
    /* overall 64 bytes long */
    uint8_t filename[FS_MAX_FNAME_LEN];
    uint32_t type;
    uint32_t inode;
    uint8_t reserved[24];
} dentry_t;

typedef struct __attribute__((packed)) fs_boot_blk_t {
    /* 64 byte header */
    uint32_t num_dentries;
    uint32_t num_inode;
    uint32_t num_data_blk;
    uint8_t reserved[52];
    dentry_t dentries[FS_MAX_DENTRIES];
} fs_boot_blk_t;

typedef struct __attribute__((packed)) inode_t {
    uint32_t file_length;
    uint32_t data_blks[FS_MAX_DBLKS];
} inode_t;

typedef uint8_t fs_data_blk_t[FS_BLOCK_SIZE];

extern fs_boot_blk_t *fs_boot_blk;
extern inode_t *inode_start;
extern fs_data_blk_t *fs_data_blk_start;

void fs_init(uint8_t *fs_start, uint8_t *fs_end);

int32_t read_dentry_by_name(const uint8_t *fname, dentry_t *dentry);
int32_t read_dentry_by_index(uint32_t index, dentry_t *dentry);
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t *buf, uint32_t length);

extern fd_open_t file_open;
extern fd_close_t file_close;
extern fd_read_t file_read;
extern fd_write_t file_write;
extern fd_open_t directory_open;
extern fd_close_t directory_close;
extern fd_read_t directory_read;
extern fd_write_t directory_write;

extern fd_driver_t file_fd_driver;
extern fd_driver_t directory_fd_driver;

#endif /* ASM */
#endif /* _FS_H */
