/* syscall.c - Implements the syscall table. */

#include "syscall.h"
#include "lib.h"


/* syscall_tbl - table for all of the syscalls*/
syscall_t *syscall_tbl[NUM_SYSCALLS] = {
    &syscall_halt,
    &syscall_execute,
    &syscall_read,
    &syscall_write,
    &syscall_open,
    &syscall_close,
    &syscall_getargs,
    &syscall_vidmap,
};
