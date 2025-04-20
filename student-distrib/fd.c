/* fd.c - Implements the file descriptor syscalls. */

#include "fd.h"
#include "syscall.h"
#include "fs.h"
#include "process.h"
#include "mm.h"
#include "lib.h"

/* syscall_read
 * Reads in data from a file descriptor into a user buffer.
 * Inputs: fd - The file descriptor index of the current process to read.
 *         nbytes - The number of bytes the user buffer supposedly can
 *                  contain
 * Outputs: buf/arg2 - The user buffer to copy the data into.
 * Return value: -1 on error, the number of bytes copied on success. EOF is given by returning
 *               zero. */
int32_t syscall_read(int32_t fd, int32_t arg2, int32_t nbytes) {
    void *buf = *(void**)&arg2;
    if(nbytes < 0 || fd < 0 || fd >= FD_PER_PROC || !buf) return -1;

    if(check_user_bounds(buf, nbytes)) return -1;

    fd_info_t *fd_info = &get_current_pcb()->fds[fd];

    if(!fd_info->present) return -1;

    return fd_info->file_ops->read(fd_info, buf, nbytes);
}

/* syscall_write
 * Writes out data to a file descriptor from a user buffer.
 * Inputs: fd - The file descriptor index of the current process to read.
 *         buf/arg2 - The user buffer to copy the data into.
 *         nbytes - The number of bytes the user buffer supposedly
 *                  contains
 * Return value: -1 on error, the number of bytes copied on success. EOF is given by returning
 *               zero. */
int32_t syscall_write(int32_t fd, int32_t arg2, int32_t nbytes) {
    const void *buf = *(const void**)&arg2;
    if(nbytes < 0 || fd < 0 || fd >= FD_PER_PROC || !buf) return -1;

    if(check_user_bounds(buf, nbytes)) return -1;

    fd_info_t *fd_info = &get_current_pcb()->fds[fd];

    if(!fd_info->present) return -1;

    return fd_info->file_ops->write(fd_info, buf, nbytes);
}

/* syscall_open
 * Tries to open a new file descriptor on the current process.
 * Inputs: filename/arg1 - The name of the file to open.
 * Return value: -1 on error, the index of the new fd on success */
int32_t syscall_open(int32_t arg1, int32_t arg2, int32_t arg3) {
    const uint8_t *filename = *(const uint8_t**)&arg1;
    if(!filename) return -1; // not strictly needed, check_user_str_bounds does this for us
    if(check_user_str_bounds(filename, FS_MAX_FNAME_LEN)) return -1;
    pcb_t *process = get_current_pcb();
    int i;
    for(i = 0; i < FD_PER_PROC; ++i) {
        if(!process->fds[i].present) break;
    }
    if(i == FD_PER_PROC) return -1; // out of file descriptors, all already present
    fd_info_t *fd_info = &process->fds[i];
    memset(fd_info, 0, sizeof(fd_info_t)); // clear it just in case
    fd_info->present = 1;
    if(file_open(fd_info, filename)) { // couldn't find file
        fd_info->present = 0;
        return -1;
    }
    return i;
}

/* syscall_close
 * Tries to close a file descriptor on the current process.
 * Inputs: fd - The file descriptor index to close
 * Return value: -1 on error, 0 on success. */
int32_t syscall_close(int32_t fd, int32_t arg2, int32_t arg3) {
    if(fd < 2 || fd >= FD_PER_PROC) return -1;
    pcb_t *process = get_current_pcb();
    fd_info_t *fd_info = &process->fds[fd];
    if(!fd_info->present) return -1;
    if(fd_info->file_ops->close(fd_info)) return -1;
    fd_info->present = 0;
    return 0;
}
