/* process.c - Implements process management functions. */

#include "process.h"
#include "lib.h"
#include "fs.h"
#include "swtch.h"
#include "idt.h"
#include "x86_desc.h"
#include "mm.h"
#include "terminal.h"

int enable_process_switching_test = 0;

static void proc_entry(void);
static void proc_entry0(void);




/* init_proc_mgmt
 * Initializes the process management system by iterating over all possible process control blocks (PCBs) 
 * and marking them as not present. This function should be called during system startup, before any processes are created.
 * Inputs: None
 * Outputs: None
 * Return value: None
 * Side effects: Modifies the present status of all PCBs in the system. The constant NUM_PROCESSES determines 
 * the maximum number of processes that can be managed.
 */
void init_proc_mgmt() {
    int i;
    for(i = 0; i < NUM_PROCESSES; ++i) {
        pcb_t *pcb = pid_to_pcb(i);
        pcb->present = 0;
    }
}



/* alloc_process
 * Allocates a process and PCB, partially initializing it, but does not switch to it
 * Inputs: parent - pointer to the parent process's PCB, can be null to indicate no parent
 *                  (i.e. to create a shell process)
 *         cmdline - null terminated string containing the full command line, including
 *                   program name and arguments. cannot be longer than ARG_LENGTH including
 *                   the null terminator
 *         terminal - TODO: CP5, this contains the terminal the process's stdin/out is
 *                    connected to
 * Return value: A pointer to the new process's PCB on success, NULL pointer on error
 * Side effects: Allocates a new process and kernel stack, reads from file system,
 *               opens some file descriptors. */
pcb_t *alloc_process(pcb_t *parent, const uint8_t *cmdline, int terminal) {
    kernel_stack_t *stack = kernel_stacks_start-1;
    int i;
    // uint32_t flags;
    // save interrupt flag since while we're modifying the PCB's
    // cli_and_save(flags);
    // find first non-present stack
    for(i = 0; i < NUM_PROCESSES && stack->pcb.present; ++i, --stack);
    if(i == NUM_PROCESSES) {
        // restore_flags(flags);
        return NULL;
    }

    pcb_t *pcb = &stack->pcb;
    pcb->terminal_id = terminal; // Store the terminal ID in the PCB
    pcb->present = 1;
    pcb->running = 1;
    pcb->vidmap = 0;
    pcb->parent = parent;
    uint8_t prog_name[ARG_LENGTH];
    i = 0;
    // skip over spaces
    while (i < ARG_LENGTH && (cmdline[i] == ' ' || cmdline[i] == '\t')) {
        ++i;
    }
    /* use separate index into prog_name, since skipping spaces in cmdline
     * would cause the start of cmdline to no longer be the same as the start
     * of prog_name */
    int j = 0;
    // copy program name to buffer
    for(; i < ARG_LENGTH; ++i, ++j) {
        if(cmdline[i] == '\0' || cmdline[i] == ' ' || cmdline[i] == '\t') {
            prog_name[j] = '\0';
            break;
        }
        prog_name[j] = cmdline[i];
    }

    uint8_t arg_buffer[ARG_LENGTH];
    // skip more spaces
    while (i < ARG_LENGTH && (cmdline[i] == ' ' || cmdline[i] == '\t')) {
        ++i;
    }
    j = 0;
    // copy arguments to arg buffer
    for (; i < ARG_LENGTH; ++i, ++j) {
        if (cmdline[i] == '\0') break;
        arg_buffer[j] = cmdline[i];
    }
    if(i == ARG_LENGTH) { /* no null terminator found in cmdline, error */
        pcb->present = 0;
        return NULL;
    }
    if(j >= ARG_LENGTH) panic_msg("j should never be past ARG_LENGTH if my logic's right");
    /* my reasoning is as follows:
     * 1) if i is greater than 0 at the start of the arg_buffer for loop, then j will always
     * stay less than i (they both get incremented at the same time), so the loop will exit
     * before j can reach ARG_LENGTH.
     * 2) if i is 0 at the start of the for loop, then cmdline[0] must be '\0', since otherwise
     * i would have been incremented by either the previous for loop or the while loop. the
     * arg_buffer for loop will then exit immediately, and j will stay at 0. */
    arg_buffer[j] = '\0';

    strncpy((int8_t*) pcb->args, (int8_t*) arg_buffer, ARG_LENGTH);

    dentry_t dentry;
    if(read_dentry_by_name(prog_name, &dentry)) {
        pcb->present = 0;
        // restore_flags(flags);
        return NULL;
    }
    if(dentry.type != FS_DENTRY_FILE) {
        pcb->present = 0;
        return NULL;
    }
    uint32_t magic;
    if(sizeof(magic) != read_data(dentry.inode, 0, (uint8_t*)&magic, sizeof(magic))) {
        pcb->present = 0;
        return NULL;
    }
    if(magic != 0x464c457f) { /* exe magic num from Appendix C of MP3 doc */
        pcb->present = 0;
        // restore_flags(flags);
        return NULL;
    }
    if(inode_start[dentry.inode].file_length > (USER_VMEM_END - USER_PROG_START)) {
        // program too big for page
        pcb->present = 0;
        return NULL;
    }

    pcb->inode = dentry.inode;

    fd_info_t *fd_info = &pcb->fds[0];
    fd_info->present = 1;
    fd_info->file_ops = &term_stdin_fd_driver;
    fd_info->file_ops->open(fd_info, (uint8_t*) "stdin");
    fd_info = &pcb->fds[1];
    fd_info->present = 1;
    fd_info->file_ops = &term_stdout_fd_driver;
    fd_info->file_ops->open(fd_info, (uint8_t*) "stdout");

    for(i = 2; i < FD_PER_PROC; ++i) {
        pcb->fds[i].present = 0;
    }
    pcb->context.esp = &stack->stack[KERNEL_STACK_SIZE];
    pcb->context.eip = &proc_entry0;
    // restore_flags(flags);
    return pcb;
}



/* kill_curr_process
 * Terminates the current process and transfers control to the parent process.
 * If the current process has no parent, it starts a new shell.
 * Inputs: exit_code - the exit code for the process
 * Outputs: None
 * Return value: Never returns (unless the current process is not present, then it does return)
 * Side effects: Modifies the running and present status of the current PCB. Closes all file descriptors of the process.
 *               If the current process has no parent, allocates a new PCB for a new shell and jumps to it.
 */
void kill_curr_process(int32_t exit_code) {
    pcb_t *process = get_current_pcb();
    if(!process->present) return;
    cli();
    // interrupts have to be disabled for this whole code, otherwise pcb pointers might go
    // invalid
    process->exit_code = exit_code;
    process->running = 0;
    int i;
    for(i = 0; i < FD_PER_PROC; ++i) {
        fd_info_t *fd = &process->fds[i];
        if(fd->present) fd->file_ops->close(fd);
    }

    pcb_t *parent = process->parent;
    if(parent != NULL) {
        parent->running = 1;
        jump_to_process(parent);
    } else {
        process->present = 0;
        // TODO CP5, pass process->terminal to child
        pcb_t *new_shell = alloc_process(NULL, (uint8_t*) "shell", process->terminal_id);
        if(!new_shell) panic_msg("unable to start new shell");
        jump_to_process(new_shell);
    }
}


/* kill_curr_process
 * Terminates the current process and transfers control to the parent process.
 * If the current process has no parent, it starts a new shell.
 * Inputs: exit_code - the exit code for the process
 * Outputs: None
 * Return value: None
 * Side effects: Modifies the running and present status of the current PCB. Closes all file descriptors of the process.
 *               If the current process has no parent, allocates a new PCB for a new shell and jumps to it.
 */
void kill_term_process(int32_t exit_code) {
    int active_terminal_id = get_active_terminal_id();
    uint32_t flags;
    cli_and_save(flags);

    pcb_t *curr_pcb = get_current_pcb();
    int need_to_jump = 0;
    int i;
    for(i = 0; i < NUM_PROCESSES; ++i) {
        pcb_t *pcb = pid_to_pcb(i);
        if(pcb->running && pcb->present && pcb->terminal_id == active_terminal_id) {
            if(curr_pcb == pcb) need_to_jump = 1;
            // The following code is taken from kill_curr_process
            pcb->exit_code = exit_code;
            pcb->running = 0;
            int j;
            for(j = 0; j < FD_PER_PROC; ++j) {
                fd_info_t *fd = &pcb->fds[j];
                if(fd->present) fd->file_ops->close(fd);
            }

            pcb_t *parent = pcb->parent;
            if(parent != NULL) {
                parent->running = 1;
            } else {
                pcb->present = 0;
                pcb_t *new_shell = alloc_process(NULL, (uint8_t*) "shell", pcb->terminal_id);
                if(!new_shell) panic_msg("unable to start new shell");
            }
        }
    }

    if(need_to_jump) do_schedule(1); // do a jump schedule to next running process

    restore_flags(flags);
}


/* proc_entry
 * Entry point for a new process. This function is called when a new process is created.
 * It sets up the user context for the new process and transfers control to it.
 * Inputs: None
 * Outputs: None
 * Return value: None
 * Side effects: Enables interrupts. Retrieves the current PCB. If process switching test is enabled, 
 *               it performs the test and halts. Initializes the user context for the new process, 
 *               sets up the TSS for the new process, and transfers control to the new process.
 */
static void proc_entry(void) {
    // TODO: set user page, finish loading in the program image, iret to program
    sti(); // context restoring leaves interrupts disabled
    pcb_t *pcb = get_current_pcb();
    if(enable_process_switching_test) {
        uint32_t pid = pcb_to_pid(pcb);
        static volatile int process_switching_test_var = 0;
        log_msg("made it to proc_entry! current PCB: %#x current PID: %u", pcb, pid);
        if(pid == 1) {
            process_switching_test_var = 1;
            switch_to_process(pid_to_pcb(0));
        }
        if(pid == 0) {
            process_switching_test_var = 2;
            switch_to_process(pid_to_pcb(1));
        }
        log_msg("made it to proc_entry! PID: %u", pid);
        if(pid == 1 && process_switching_test_var == 2) log_msg("test PASS!");
        else log_msg("test FAIL!");
        while(1) asm volatile("hlt");
    }


    uint32_t pid = pcb_to_pid(pcb);
    set_user_page(pid);

    if(0 > read_data(pcb->inode, 0, (uint8_t*) USER_PROG_START, PAGE_4M_SIZE)) {
        panic_msg("huh? unable to read program image?");
    }

    iret_context_user_t uctx;
    memset(&uctx, 0, sizeof(uctx));
    uctx.base.ds = uctx.base.es = uctx.base.fs = uctx.base.gs = uctx.ss = USER_DS;
    uctx.base.cs = USER_CS;
    uctx.base.eip = *(uint32_t*)(USER_PROG_START + 24);
    uctx.esp = USER_VMEM_END;
    // Only enable the interrupt flag and the reserved 1 bit in eflags for users initially
    // See x86 manual vol 3 sec 2.3 for important system flags
    /* vol 1 sec 3.4.3 has descriptions for the status and control flags (which we can
     * just leave as zero) */
    uctx.base.eflags = 0x202;
    cli(); // make sure our modifications to tss don't get changed
    tss.esp0 = (uint32_t)(((kernel_stack_t*)pcb) + 1);
    // I'm pretty sure writing to tss in memory is enough and no ltr instruction is needed
    pop_iret_context(&uctx.base);
}


/* proc_entry0
 * Calls proc_entry()
 * Inputs: None
 * Outputs: None
 * Return value: None
 * Side effects: Enables interrupts. Retrieves the current PCB. If process switching test is enabled, 
 *               it performs the test and halts. Initializes the user context for the new process, 
 *               sets up the TSS for the new process, and transfers control to the new process.
 */
static void proc_entry0(void) {
    // note! esp points to nothing!! don't return!
    /* added an extra call so that GDB doesn't freak out over not having an old EIP on
     * the stack. this might get optimized out by the compiler if optimizations are
     * enabled, but eh, it doesn't affect functionality at all. */
    proc_entry();
    panic_msg("proc_entry returned!");
}


/* switch_to_process
 * Switches the CPU context from the current process to the specified process.
 * Inputs: pcb - pointer to the PCB of the process to switch to
 * Outputs: None
 * Return value: 0 on success, -1 if the specified PCB is NULL or not present
 * Side effects: Modifies the CPU context to switch to the specified process. Updates the esp0 field of the TSS.
 *               If the current PCB is not present, it causes a panic.
 */
int32_t switch_to_process(pcb_t *pcb) {
    if(!pcb || !pcb->present) return -1;
    pcb_t *curr_pcb = get_current_pcb();
    if(pcb == curr_pcb) return 0; // shortcut if it's the same process
    if(!curr_pcb->present) panic_msg("current pcb must be present!");
    uint32_t flags;
    cli_and_save(flags);
    swap_context(&curr_pcb->context, &pcb->context);
    set_user_page(pcb_to_pid(curr_pcb));
    tss.esp0 = (uint32_t)(((kernel_stack_t*)curr_pcb) + 1);
    restore_flags(flags);
    return 0;
}



/* jump_to_process
 * Restores the CPU context from the specified process control block (PCB) and jumps to that process.
 * Inputs: pcb - pointer to the PCB of the process to jump to
 * Outputs: None
 * Return value: 0 on success, -1 if the specified PCB is NULL or not present
 * Side effects: Restores the CPU context from the specified PCB. The function does not return 
 *               if the context is successfully restored.
 */
int32_t jump_to_process(pcb_t *pcb) {
    if(!pcb || !pcb->present) return -1;
    restore_context(&pcb->context);
    return 0; // never runs
}

/* do_schedule
 * Switches to the next running process after the current one so as to perform
 * round robin scheduling. If none of the processes are running, it halts, waiting for
 * an interrupt to occur, then checks again.
 * Inputs: jump - Boolean, non-zero to call jump_to_process, zero to call switch_to_process
 * Return value: void if jump is false, never returns otherwise
 * Side effects: Calls (switch|jump)_to_process, so the we must currently be switched in to a
 * process (i.e. not on the initial kernel stack). */
void do_schedule(int jump) {
    uint32_t flags;
    cli_and_save(flags);

    pcb_t *curr_pcb = get_current_pcb();
    uint32_t curr_pid = pcb_to_pid(curr_pcb);
    if(!jump && !curr_pcb->present) panic_msg("switch without current process present!");
    do {
        int switched = 0;
        uint32_t i;
        for(i = 0; i < NUM_PROCESSES; ++i) {
            pcb_t *next = pid_to_pcb((curr_pid + i + 1) % NUM_PROCESSES);
            if(next->present && next->running) {
                if(jump) jump_to_process(next);
                else switch_to_process(next);
                switched = 1;
                break; // break out of inner for loop and check if we're runnable
            }
        }
        if(!switched) {
            // if we didn't find any running processes, wait until next hardware interrupt
            asm volatile ("sti; hlt; cli");
        }
    } while(jump || !(curr_pcb->present && curr_pcb->running));

    restore_flags(flags);
}

/* syscall_execute
 * Executes a new process with the specified command.
 * Inputs: arg1 - pointer to the command string
 *         arg2 - not used
 *         arg3 - not used
 * Outputs: None
 * Return value: the exit code of the child process, or -1 if the command is NULL or exceeds the maximum length, 
 *               or if a new process cannot be allocated
 * Side effects: Disables interrupts. Allocates a new PCB for the child process and switches to it. 
 *               Sets the running status of the current process to 0. If the new process cannot be switched to, 
 *               it causes a panic.
 */
int32_t syscall_execute(int32_t arg1, int32_t arg2, int32_t arg3) {
    const uint8_t *command = *(const uint8_t**) &arg1;
    if(!command) return -1;
    if(check_user_str_bounds(command, ARG_LENGTH-1)) return -1;
    pcb_t *current = get_current_pcb();

    uint32_t flags;
    cli_and_save(flags);
    /* disable interrupts so that processes dont disappear under our feet, and so we don't
     * get stuck with current->running off, never to be scheduled again */

    // TODO CP5, pass current->terminal to child
    pcb_t *child = alloc_process(get_current_pcb(), command, current->terminal_id);
    if(!child) {
        restore_flags(flags);
        return -1;
    }
    current->running = 0;
    // the parent running flag gets set by kill_curr_process
    if(switch_to_process(child)) panic_msg("unable to switch to new process!");

    // at this point, child will have its exit_code set
    int32_t exit_code = child->exit_code;
    child->present = 0;
    restore_flags(flags);
    return exit_code;
}


/* syscall_halt
 * Terminates the current process and returns control to the parent process.
 * Inputs: arg1 - the exit status of the current process
 *         arg2 - not used
 *         arg3 - not used
 * Outputs: None
 * Return value: 0 on success, -1 if the exit status is less than 0 or greater than MAX_USER_STATUS
 * Side effects: Terminates the current process. If the exit status is valid, it is passed to the parent process.
 */
int32_t syscall_halt(int32_t arg1, int32_t arg2, int32_t arg3) {
    // should truncate it to the 8 least significant bits.
    // alternatively, could use a pointer access to the first byte of arg1.
    uint8_t ret_val = (uint8_t) (uint32_t) arg1;
    kill_curr_process(ret_val);
    return 0; // never runs
}


/*
* syscall_getargs(uint8_t* buf, int32_t nbytes)
* DESCRIPTION: Copies the arguments of the current process into the buffer
* INPUTS: buf/arg1 - buffer to copy the arguments into
*         nbytes/arg2 - number of bytes to copy
* OUTPUTS: buf/arg1
* RETURNS: 0 on success, -1 on failure
*/
int32_t syscall_getargs(int32_t arg1, int32_t arg2, int32_t arg3) {
    uint8_t* buf = (uint8_t*)arg1;
    int32_t nbytes = arg2;
    if(!buf || nbytes < 0) return -1;
    if(check_user_bounds(buf, nbytes) == -1) return -1;
    pcb_t *current = get_current_pcb();
    if(current->present == 0) return -1;
    uint32_t arglen = strlen((int8_t*) current->args);
    if(arglen+1 > nbytes || !arglen) return -1; // return -1 if no arguments
    strncpy((int8_t*) buf, (int8_t*) current->args, nbytes);
    return 0;
}
