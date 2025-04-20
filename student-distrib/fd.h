/* fd.h -- contains definitions for the file descriptor API */

#ifndef _FD_H
#define _FD_H

#include "types.h"

#ifndef ASM

typedef struct fd_driver_t fd_driver_t;

/* fd_info_t
 * struct containing all the information that is specfic to a file descriptor,
 * including stdin/out file descriptors, device driver file descriptors, regular file
 * file descriptors, and directory file descriptors. includes all the fields listed
 * in section 2 of appendix A of the mp3 document, plus space for driver specific data */
typedef struct fd_info_t {
    /* matches section 2 of Appendix A of MP3 document */
    fd_driver_t *file_ops;
    uint32_t inode;
    uint32_t file_pos;
    uint32_t present : 1;
    uint32_t flags : 31;
    /* individual file descriptor drivers can store whatever in driver_data,
     * can expand the array at a later point if needed */
    uint32_t driver_data[4];
} fd_info_t;

/* abstract types for each of the syscalls file descriptor drivers should implement */
/* fd_open_t does not return the file descriptor number, just success or failure (0/-1) */
/* all of the other ones (fd_close_t, fd_read_t, fd_write_t) return according to Appendix B
 * of the MP3 document */
/* fd_open_t
 * initializes a file descriptor, setting the file_ops, inode, file_pos, and driver_data as
 * needed
 * Inputs: filename -- the name of the file to open, as a null terminated string
 * Outputs: fd_info -- pointer to a struct where the driver should initialize the file
 *                     descriptor info
 * Return value -- 0 on success, -1 on error
 * Side effects: depends on the driver */
typedef int32_t fd_open_t(fd_info_t *fd_info, const uint8_t *filename);
/* fd_close_t
 * Performs any device specific procedures for closing a file descriptor
 * Inputs: fd_info -- the file descriptor info struct to deinitialize
 * Outputs: none
 * Return value: 0 on success, -1 on error
 * Side effects: depends on driver */
typedef int32_t fd_close_t(fd_info_t *fd_info);
/* fd_read_t
 * Reads nbytes from the file / driver into buf at the current position in the file
 * Inputs: fd_info -- the file descriptor info struct corresponding to the file / device
 *                    we should read from
 *         nbytes -- the maximum number of bytes we should copy into buf
 * Outputs: buf -- the buffer we should copy data into, must have storage for at least
 *                 nbytes bytes
 * Return value: -1 on error, otherwise the number of bytes actually written to the start
 *               of buf
 * Side effects: Writes to buf, additional effects depending on driver. Might wait for an
 *               interrupt / state change depending on driver. Updates the file_pos */
typedef int32_t fd_read_t(fd_info_t *fd_info, void *buf, int32_t nbytes);
/* fd_write_t
 * Writes nbytes to the file / driver from buf.
 * Inputs: fd_info -- the file descriptor info struct corresponding to the file / device
 *                    we are writing to
 *         buf -- the buffer we are copying from, must have at least nbytes bytes at the start
 *                of it.
 *         nbytes -- the maximum number of bytes to write from the provided buffer to
 *                   the driver
 * Outputs: none
 * Return value: -1 on error, otherwise the actual number of bytes written to the
 *               file / driver
 * Side effects: depends on the driver, may wait depending on the driver (though none
 *               of ours will as far as I know) */
typedef int32_t fd_write_t(fd_info_t *fd_info, const void *buf, int32_t nbytes);

/* static structs for function pointers to a given fd driver's API */
/* fd_driver_t
 * A struct containing function pointers to each of the driver file descriptor operations.
 * Each driver should define a static variable of this type containing its operations. */
struct fd_driver_t {
    fd_open_t *open;
    fd_close_t *close;
    fd_read_t *read;
    fd_write_t *write;
};

#endif /* ASM */
#endif /* _FD_H */
