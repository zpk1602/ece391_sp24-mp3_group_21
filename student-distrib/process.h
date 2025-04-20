/* process.h - Contains definitions relating to process management */

#ifndef _PROCESS_H
#define _PROCESS_H

#include "types.h"
#include "fd.h"
#include "swtch.h"
#include "syscall.h"

/* 8KiB kernel stacks */
#define KERNEL_STACK_SIZE (1 << 13)
/* 8 file descriptors allowed per process, including stdin and stdout */
#define FD_PER_PROC 8
/* 6 processes max for now */
#define NUM_PROCESSES 6

/* maximum length that the command line invoking a program can be (including
 * executable file name at the start) */
#define ARG_LENGTH 128

#define MAX_USER_STATUS 255
#define EXCEPTION_STATUS 256
#define TERMINATED_STATUS 257

/* Address to which program image is copied. */
#define USER_PROG_START 0x08048000

#ifndef ASM

typedef struct pcb_t pcb_t;
struct pcb_t {
    pcb_t *parent;
    context_t context;
    uint32_t present : 1;
    /* flag for whether the process can be run by the scheduler */
    uint32_t running : 1;
    uint32_t vidmap : 1;
    uint32_t flags : 29;
    int32_t exit_code;
    fd_info_t fds[FD_PER_PROC];
    uint8_t args[ARG_LENGTH];
    uint32_t inode;
    /* terminal ID */
    int terminal_id;
};

typedef struct kernel_stack_t kernel_stack_t;
struct kernel_stack_t {
    union {
        pcb_t pcb;
        uint8_t stack[KERNEL_STACK_SIZE];
    };
};

/* init_proc_mgmt
 * Initializes the process manager, including setting up the PCB's to be not present */
void init_proc_mgmt();

/* alloc_process
 * Tries to allocate a process, starting it with the given command line (which includes
 * the executable file name at the start). Does not actually begin running the process,
 * to do that, switch to the process via other functions.
 * Interrupts should be disabled when running this function, so that the PCB pointers
 * don't go invalid due to the process finishing.
 * Return value: Returns the PCB on success, NULL on error */
pcb_t *alloc_process(pcb_t *parent, const uint8_t *cmdline, int terminal);

/* kill_curr_process
 * Kills the current process.
 * Sets exit code, closes file descriptors, switches to parent process if there is one,
 * otherwise respawns the process.
 * Never returns. */
void kill_curr_process(int32_t exit_code);


/* kill_term_process
 * Kills the current process on the active terminal.
 * Sets exit code, closes file descriptors, switches to parent process if there is one,
 * otherwise respawns the process.
 * Never returns. */
void kill_term_process(int32_t exit_code);

/* enable_process_switching_test
 * Enables the process switching test, which tests switching back and forth a few times
 * between a few processes executing in kernel mode. */
extern int enable_process_switching_test;

/* kernel stacks grow downward from the end of the kernel page */
static kernel_stack_t *const kernel_stacks_start = (kernel_stack_t *) 0x800000;

/* various functions for converting between pid's, pcb pointers, and stack pointers */
static inline pcb_t *pid_to_pcb(uint32_t pid) {
    return &(kernel_stacks_start - (pid+1))->pcb;
}
static inline uint32_t pcb_to_pid(pcb_t *pcb) {
    return (kernel_stacks_start - ((kernel_stack_t*) pcb)) - 1;
}
static inline pcb_t *get_current_pcb() {
    uint32_t esp;
    asm volatile ("movl %%esp, %0" : "=g"(esp));
    // subtract one so that if the kernel stack is empty, we'll still return the right pcb
    return (pcb_t*)((esp-1) & ~(KERNEL_STACK_SIZE-1));
}

/* switch_to_process
 * Tries to switch to the given PCB, returns 0 on success (after we switch back)
 * Return value: 0 on success (after switching back later), -1 on error
 *               (ie pcb isn't present) */
int32_t switch_to_process(pcb_t *pcb);

/* jump_to_process
 * Tries to jump to the given PCB, without saving the current one's state.
 * Used for starting the first process from kernel.c entry(), and for halting a process
 * Return value: never returns */
int32_t jump_to_process(pcb_t *pcb);

/* do_schedule
 * Switches to the next running process after the current one so as to perform
 * round robin scheduling. If none of the processes are running, it halts, waiting for
 * an interrupt to occur, then checks again. Uses jump_to_process if jump is true,
 * otherwise uses switch_to_process. */
void do_schedule(int jump);


#endif /* ASM */
#endif /* _PROCESS_H */
