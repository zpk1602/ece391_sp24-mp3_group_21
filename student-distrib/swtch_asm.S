/* swtch_asm.S - Implements stack switching. */

#define ASM
#include "swtch.h"

/* field offsets for context_t struct from swtch.h */
#define ESP 0
#define EIP 4

.globl restore_context


/* void restore_context(context_t *restore)
 * C calling convention
 * restores the context of the given context_t struct
 * Inputs: restore - the context to jump to
 * Return value: never returns
 * Side effects: Starts executing the code at the given eip and with the stack at
 *               the given esp. */
restore_context:
    /* though not strictly needed, the following can help with debugging, allowing
     * us to see across stack switches in the debugger */
    pushl %ebp
    movl %esp, %ebp
    // see comment in swap_context for why we cli here
    cli
    // load first arg (context_t pointer) into EAX
    movl 8(%ebp), %eax
    // load the new context
    movl ESP(%eax), %esp
    jmp *EIP(%eax)

.globl swap_context
/* void swap_context(context_t *save, context_t *restore)
 * C calling convention
 * saves the current execution context to save, then jumps to the context given in restore.
 * Inputs: save - the context struct to save the current execution context to
 *         restore - the context to jump to
 * Return value: none
 * Side effects: Starts executing the code at the given eip and with the stack at
 *               the given esp. Sets save to then return back to this function, which
 *               then returns when someone else restores the save context_t */
swap_context:
    pushl %ebp
    movl %esp, %ebp
    /* by pushing flags and disabling interrupts, we ensure that we will never
     * be in an interrupt handler without interrupts being disabled (prevents
     * unbounded stack due to nested interrupts) */
    pushfl
    cli
    // save callee saved registers
    pushl %ebx
    pushl %esi
    pushl %edi

    // load first arg, context_t struct pointer to save current context to
    movl 8(%ebp), %esi
    // save landing context
    movl %esp, ESP(%esi)
    movl $.Lreturn_landing, EIP(%esi)

    // load second arg, context_t struct pointer to jump to
    movl 12(%ebp), %edi
    movl ESP(%edi), %esp
    jmp *EIP(%edi)

/* return landing - cleans up from swapping to other contexts.*/
// use local variable that does not show up in symbol table
.Lreturn_landing:
    // welcome back! hope your trip to other contexts was fun!
    // restore callee saved registers
    popl %edi
    popl %esi
    popl %ebx
    // restore interrupt flag
    popfl
    popl %ebp
    ret

// used by gdb_scripts for stack tracing other kernel stacks
// return_landing has to be a local variable so that GDB can understand
// the stack frame of swap_context. because of this, we need some other
// way to get the address of return_landing in the gdb_scripts.gdb file.
// that is the purpose of this symbol.
.globl swap_context_return_landing_addr
swap_context_return_landing_addr:
    .long .Lreturn_landing
