/* syscall.h - Syscall dispatch definitions */

#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "idt.h"

#define NUM_SYSCALLS 10

#ifndef ASM

typedef int32_t syscall_t(int32_t arg1, int32_t arg2, int32_t arg3);

/*
1. int32_t halt (uint8_t status);
2. int32_t execute (const uint8_t* command);
3. int32_t read (int32 t_fd, void* buf, int32_t nbytes);
4. int32_t write (int32 t_fd, const void* buf, int32_t nbytes);
5. int32_t open (const uint8_t* filename);
6. int32_t close (int32_t fd);
7. int32_t getargs (uint8_t* buf, int32_t nbytes);
8. int32_t vidmap (uint8_t** screen start);
9. int32_t set handler (int32_t signum, void* handler_address);
10. int32_t sigreturn (void);
*/

extern syscall_t syscall_halt; // In process.c
extern syscall_t syscall_execute; // In process.c
extern syscall_t syscall_read; // In fd.c
extern syscall_t syscall_write; // In fd.c
extern syscall_t syscall_open; // In fd.c
extern syscall_t syscall_close; // In fd.c
extern syscall_t syscall_getargs; // In process.c
extern syscall_t syscall_vidmap; // In mm.c

/* syscall_tbl
 * Jump table for the syscalls, syscall number i maps to index i-1 in this array
 * (since the signal numbers are 1-indexed while the array is 0-index). */
extern syscall_t *syscall_tbl[NUM_SYSCALLS];

#endif /* ASM */
#endif /* _SYSCALL_H */
