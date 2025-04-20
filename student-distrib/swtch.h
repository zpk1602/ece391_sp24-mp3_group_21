/* swtch.h - Contains definitions for the stack switching functions */

#ifndef _SWTCH_H
#define _SWTCH_H

#include "types.h"

#ifndef ASM

/* struct containing a context that we can jump to.
 * The state of the processor after restoring a context (ie via restore_context)
 * is EIP set to the value in the struct, ESP set to the value in the struct,
 * interrupts disabled, and all other registers/state are left undefined.
 * See restore_context implementation in swtch.S for how contexts are exactly
 * restored. */
typedef struct context_t context_t;
struct context_t {
    void *esp;
    void *eip;
};

/* custom_ctx_fn_t
 * Function type for custom context functions, as used by make_context().
 * buf is a pointer to the buffer of length buf_len passed in on the new
 * stack. These functions should never return, if they do, the kernel will panic
 * on a null pointer derefence exception. Hardware interrupts will be disabled upon
 * calling this function; reenable them if desired via sti(). */
typedef void custom_ctx_fn_t(void *buf, uint32_t buf_len);

/* make_context
 * Makes a custom context that when jumped to runs the given function on the given stack.
 * The given buffer is copied to the new context's stack, and is then passed as
 * a pointer to the function. Returning from the function causes null dereference.
 * esp is a pointer to the end of the stack's buffer, ie the first thing pushed on the
 * stack will be at (esp-4).
 * NOTE: this function modifies the stack at the passed in esp!!! do not use with
 * esp pointing to a stack that is currently in use! (i.e. the initial kernel stack) */
void make_context(context_t *ctx, void *esp, custom_ctx_fn_t *fn,
        void *buf, uint32_t buf_len);

/* restore_context
 * Jumps to the given context without saving the current context.
 * Never returns.
 * Useful when, say, halting a process. */
void restore_context(context_t *restore);

/* swap_context
 * Saves the current context to save (including pushing callee saved registers
 * to the stack and saving state of interrupt flag) before jumping to another context.
 * Only returns once the saved context gets restored. */
void swap_context(context_t *save, context_t *restore);

#endif /* ASM */
#endif /* _SWTCH_H */
