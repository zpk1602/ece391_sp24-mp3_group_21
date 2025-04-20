/* fs.c -- filesystem driver implementation */

#include "lib.h"
#include "fs.h"
#include "rtc.h"

/* Pointer to the boot block of the filesystem multiboot module */
fs_boot_blk_t *fs_boot_blk;
/* Pointer to the first inode block of the filesystem multiboot module */
inode_t *inode_start;
/* Pointer to the first data block of the filesystem multiboot module */
fs_data_blk_t *fs_data_blk_start;

/* fs_init
 * Initializes the filesystem driver, setting up pointers to various parts of the
 * filesystem in memory using the location of the filesystem multiboot module.
 * Inputs: fs_start -- Pointer to the first byte of the filesystem multiboot module
 *         fs_end -- Pointer to one past the last byte of the filesystem multiboot module
 * Outputs / Return value: none
 * Side effects: Sets up driver internal pointers to various parts of the filesystem in
 *               memory, panics if various invariants about the filesystem are not satisfied. */
void fs_init(uint8_t *fs_start, uint8_t *fs_end) {
    log_msg("filesystem start: 0x%#x end: 0x%#x", fs_start, fs_end);
    /* double check lengths of structs */
    if(sizeof(fs_boot_blk_t) != FS_BLOCK_SIZE) {
        panic_msg("fs_boot_blk_t size was %d should be %d!",
                sizeof(fs_boot_blk_t), FS_BLOCK_SIZE);
    }
    if(sizeof(inode_t) != FS_BLOCK_SIZE) {
        panic_msg("inode_t size was %d should be %d!",
                sizeof(inode_t), FS_BLOCK_SIZE);
    }
    if(sizeof(fs_data_blk_t) != FS_BLOCK_SIZE) {
        panic_msg("fs_data_blk_t size was %d should be %d!",
                sizeof(fs_data_blk_t), FS_BLOCK_SIZE);
    }
    if(sizeof(dentry_t) != FS_DENTRY_SIZE) {
        panic_msg("dentry_t size was %d should be %d!",
                sizeof(dentry_t), FS_DENTRY_SIZE);
    }
    if(!fs_start || !fs_end) {
        panic_msg("null fs_start or fs_end!");
    }
    uint32_t num_blocks = (fs_end - fs_start) >> 12;
    if(num_blocks < 1) {
        panic_msg("filesystem has no blocks!");
    }
    fs_boot_blk = (fs_boot_blk_t*) fs_start;
    if(num_blocks < 1 + fs_boot_blk->num_inode + fs_boot_blk->num_data_blk) {
        panic_msg("filesystem extends past end of module!");
    }
    inode_start = (inode_t*) fs_boot_blk + 1;
    fs_data_blk_start = (fs_data_blk_t*) inode_start + fs_boot_blk->num_inode;
    log_msg("fs boot blk: %#x inodes: %#x data blks: %#x",
            fs_boot_blk, inode_start, fs_data_blk_start);
}

/* read_dentry_by_name
 * Reads a directory entry into the provided struct given the filename of the entry,
 * searching through the boot block to find it.
 * Inputs: fname -- the name of the file to read its dentry, as a null terminated string
 * Outputs: dentry -- pointer to a struct to store the dentry into (note, makes a copy
 *                    of the dentry in the provided struct; does not return address
 *                    of the original dentry in the filesystem's memory)
 * Return value: 0 on success, -1 on error or if the file could not be found
 * Side effects: Copies to the provided dentry struct, otherwise none */
int32_t read_dentry_by_name(const uint8_t *fname, dentry_t *dentry) {
    int i,j;
    dentry_t *curr = &fs_boot_blk->dentries[0];
    if(!fname || !dentry) return -1;
    for(i = 0; i < fs_boot_blk->num_dentries; ++i, ++curr) {
        /* iterate only over first FS_MAX_FNAME_LEN bytes, since that's all we care about */
        /* by only checking these bytes, and not using strlen, we ensure bad user
         * pointers won't cause issues */
        for(j = 0; j < FS_MAX_FNAME_LEN; ++j) {
            if(fname[j] != curr->filename[j])
                goto next_outer_loop;
            if(fname[j] == '\0') goto match;
        }
        /* full 32 byte name; check that fname is also 32 bytes */
        if(fname[FS_MAX_FNAME_LEN] != '\0')
            continue;

        match:;
        memcpy(dentry, curr, FS_DENTRY_SIZE);
        return 0;

        next_outer_loop:;
    }
    return -1;
}

/* read_dentry_by_index
 * Copies a dentry into the provided struct given its index in the filesystem's boot block
 * Inputs: index -- the zero-based index into the filesystem's root directory. this should
 *                  be no larger than the num_dentries field in the boot block minus 1
 * Outputs: dentry -- Pointer to where in memory the dentry struct should be copied to
 * Return value: 0 on success, -1 on error (i.e. if the index was out of bounds)
 * Side effects: Copies into the provided struct, otherwise none */
int32_t read_dentry_by_index(uint32_t index, dentry_t *dentry) {
    if(!dentry) return -1;
    if(index >= fs_boot_blk->num_dentries) return -1;
    memcpy(dentry, &fs_boot_blk->dentries[index], FS_DENTRY_SIZE);
    return 0;
}

/* read_data
 * Reads a chunk of data from the given inode at the given offset within the file
 * Inputs: inode -- The index into the array of inodes in the filesystem to read from
 *         offset -- The offset within the file where we should be reading from
 *         length -- The amount of bytes to at most copy into buf
 * Outputs: buf -- A buffer to store data from the file into. It should have space
 *                 for storing at least length bytes
 * Return value: -1 on error (i.e. the inode was out of bounds), otherwise the number
 *               of bytes copied over (0 for end of file or if length is 0)
 * Side effects: Copies into the provided buf at indices ranging from 0 up to length-1,
 *               otherwise none */
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t *buf, uint32_t length) {
    /* an important note about this code:
     * one main objective is to ensure that arbitrary parameters should fail safely,
     * which means we have to keep in mind the possibility of overflows throughout
     * this function. */
    if(inode >= fs_boot_blk->num_inode) return -1;
    if(buf == NULL) return -1;
    inode_t *in_ptr = inode_start + inode;

    /* overall, this algorithm loops over each data block, copying whole 4KiB blocks
     * using memcpy(). it handles all edge cases by only moving the minimum of
     * three different limitations: the length of the given buffer, the length
     * of the data block, and the length of the file. since it only uses subtraction
     * and adding the number of bytes copied, it cannot overflow */
    uint32_t i = 0; /* bytes read so far */
    while(i < length && offset < in_ptr->file_length) {
        /* handle next data block */
        uint32_t blk_idx = offset >> FS_DATA_BLK_BITS;
        /* file length suggests more data blocks than there actually are in inode */
        if(blk_idx >= FS_MAX_DBLKS) return -1;
        blk_idx = in_ptr->data_blks[blk_idx];
        /* data block index in inode extends past actual data blocks */
        if(blk_idx >= fs_boot_blk->num_data_blk) return -1;
        /* start point within block */
        uint32_t start = offset & (FS_BLOCK_SIZE-1);
        /* read the minimum of (length - i), (FS_BLOCK_SIZE - start), and
         * (file_length - offset) bytes */
        uint32_t count = length - i;
        if(count > (FS_BLOCK_SIZE - start))
            count = FS_BLOCK_SIZE - start;
        if(count > (in_ptr->file_length - offset))
            count = in_ptr->file_length - offset;
        memcpy(buf, fs_data_blk_start[blk_idx] + start, count);
        buf += count;
        /* incrementing i won't overflow, since i won't go past length due
         * to count being no greater than (length-i) */
        i += count;
        /* offset won't overflow, since it won't go past the file length due
         * to count being no greater than (file_length - offset) */
        offset += count;
    }
    return i;
}

/* file_open
 * fd_open_t function for the filesystem, main entrypoint for opening files, directories,
 * and devices based on their filename in the filesystem. calls device specific open calls
 * for directories and non-regular files
 * Inputs: filename -- the name of the file to open, as a null terminated string
 * Outputs: fd_info -- pointer to a struct where the driver should initialize the file
 *                     descriptor info
 * Return value -- 0 on success, -1 on error
 * Side effects: side effects of rtc_open, otherwise none */
int32_t file_open(fd_info_t *fd_info, const uint8_t *filename) {
    if(!fd_info || !filename) return -1;
    dentry_t dentry;
    if(read_dentry_by_name(filename, &dentry)) return -1;
    switch(dentry.type) {
    case FS_DENTRY_RTC:
        if(dentry.inode != 0) return -1;
        return rtc_fd_driver.open(fd_info, filename);
    case FS_DENTRY_DIR:
        if(dentry.inode != 0) return -1;
        return directory_fd_driver.open(fd_info, filename);
    case FS_DENTRY_FILE:
        fd_info->file_ops = &file_fd_driver;
        fd_info->inode = dentry.inode;
        fd_info->file_pos = 0;
        return 0;
    default:
        return -1;
    }
}
/* file_close
 * fd_close_t function for closing regular files, currently does nothing
 * Inputs: fd_info -- the file descriptor info struct to deinitialize
 * Outputs: none
 * Return value: 0 on success, -1 on error
 * Side effects: none */
int32_t file_close(fd_info_t *fd_info) {
    if(!fd_info) return -1;
    return 0;
}
/* file_read
 * fd_read_t function for reading from regular files
 * Inputs: fd_info -- the file descriptor info struct corresponding to the regular file
 *                    we should read from
 *         nbytes -- the maximum number of bytes we should copy into buf
 * Outputs: buf -- the buffer we should copy data into, must have storage for at least
 *                 nbytes bytes
 * Return value: -1 on error, otherwise the number of bytes actually written to the start
 *               of buf
 * Side effects: Writes to buf, updates the file_pos */
int32_t file_read(fd_info_t *fd_info, void *buf, int32_t nbytes) {
    if(!fd_info || !buf) return -1;
    if(nbytes < 0) return -1;
    int32_t count_read = read_data(fd_info->inode, fd_info->file_pos,
            buf, nbytes);
    if(count_read > 0) fd_info->file_pos += count_read;
    return count_read;
}
/* file_write
 * fd_write_t function for regular files, always returns error since the filesystem is
 * read only.
 * Inputs: fd_info -- the file descriptor info struct corresponding to the file / device
 *                    we are writing to
 *         buf -- the buffer we are copying from, must have at least nbytes bytes at the start
 *                of it.
 *         nbytes -- the maximum number of bytes to write from the provided buffer to
 *                   the driver
 * Outputs: none
 * Return value: -1 on error, otherwise the actual number of bytes written to the
 *               file / driver
 * Side effects: none */
int32_t file_write(fd_info_t *fd_info, const void *buf, int32_t nbytes) {
    return -1; /* not supported, read only filesystem */
}

/* directory_open
 * fd_open_t function for the single directory, initializes the fd_info to be a directory
 * file descriptor
 * Inputs: filename -- the name of the file to open, as a null terminated string (not used)
 * Outputs: fd_info -- pointer to a struct where the driver should initialize the file
 *                     descriptor info
 * Return value -- 0 on success, -1 on error
 * Side effects: none */
int32_t directory_open(fd_info_t *fd_info, const uint8_t *filename) {
    if(!fd_info || !filename) return -1;
    fd_info->file_ops = &directory_fd_driver;
    fd_info->inode = 0;
    fd_info->file_pos = 0;
    return 0;
}
/* directory_close
 * fd_close_t function for closing directory file descriptors, currently does nothing
 * Inputs: fd_info -- the file descriptor info struct to deinitialize
 * Outputs: none
 * Return value: 0 on success, -1 on error
 * Side effects: none */
int32_t directory_close(fd_info_t *fd_info) {
    if(!fd_info) return -1;
    return 0;
}
/* directory_read
 * fd_read_t function for the single directory, reads the name from one dentry into the
 * provided buffer, then advances to the next dentry (directory entry). The name is exactly
 * as it is in the dentry, i.e. 32 bytes of null padded text (note: the name may take up
 * the full 32 bytes, in which cacse there will be no null character)
 * Inputs: fd_info -- the file descriptor info struct corresponding to the file / device
 *                    we should read from
 *         nbytes -- the maximum number of bytes we should copy into buf
 * Outputs: buf -- the buffer we should copy data into, must have storage for at least
 *                 nbytes bytes
 * Return value: -1 on error, otherwise the number of bytes actually written to the start
 *               of buf
 * Side effects: Writes to buf. Updates the file_pos */
int32_t directory_read(fd_info_t *fd_info, void *buf, int32_t nbytes) {
    if(!fd_info || !buf) return -1;
    if(nbytes < 0) return -1;
    if(fd_info->file_pos >= fs_boot_blk->num_dentries) return 0;
    dentry_t dentry;
    if(read_dentry_by_index(fd_info->file_pos++, &dentry)) return -1;
    /* huh?? negative number of bytes? shrugs */
    /* TODO might have to change this return value for negative nbytes */
    if(nbytes > 32) nbytes = 32;
    memcpy(buf, dentry.filename, nbytes);
    return nbytes;
}
/* directory_write
 * fd_write_t function for the directory, always returns error since the filesystem is
 * read only.
 * Inputs: fd_info -- the file descriptor info struct corresponding to the file / device
 *                    we are writing to
 *         buf -- the buffer we are copying from, must have at least nbytes bytes at the start
 *                of it.
 *         nbytes -- the maximum number of bytes to write from the provided buffer to
 *                   the driver
 * Outputs: none
 * Return value: -1 on error, otherwise the actual number of bytes written to the
 *               file / driver
 * Side effects: none */
int32_t directory_write(fd_info_t *fd_info, const void *buf, int32_t nbytes) {
    return -1; /* not supported, read only filesystem */
}

/* file_fd_driver
 * A struct containing function pointers to each of the driver file descriptor operations
 * for regular files. */
fd_driver_t file_fd_driver = {
    .open = file_open,
    .close = file_close,
    .read = file_read,
    .write = file_write,
};

/* directory_fd_driver
 * A struct containing function pointers to each of the driver file descriptor operations
 * for the directory. */
fd_driver_t directory_fd_driver = {
    .open = directory_open,
    .close = directory_close,
    .read = directory_read,
    .write = directory_write,
};
