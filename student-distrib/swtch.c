/* swtch.c - Implements making a custom context. */

#include "swtch.h"
#include "lib.h"





/* make_context
 * Makes a custom context that when jumped to runs the given function on the given stack.
 * The given buffer is copied to the new context's stack, and is then passed as
 * a pointer to the function. Returning from the function causes null dereference.
 * esp is a pointer to the end of the stack's buffer, ie the first thing pushed on the
 * stack will be at (esp-4).
 * NOTE: this function modifies the stack at the passed in esp!!! do not use with
 * esp pointing to a stack that is currently in use! (i.e. the initial kernel stack) */
void make_context(context_t *ctx, void *esp, custom_ctx_fn_t *fn,
        void *buf, uint32_t buf_len) {
    // push provided buf to the new stack
    esp -= buf_len;
    esp = (void*)(((uint32_t)esp) & ~3); // align esp down to 4 byte boundary
    void *stack_buf = esp;
    memcpy(stack_buf, buf, buf_len);
    // setup C calling convention frame for return EIP and two args
    esp -= 12;
    *(void**)(esp+0) = NULL; // old EIP, null in our case to cause exception
    *(void**)(esp+4) = stack_buf; // first arg of fn, buf
    *(uint32_t*)(esp+8) = buf_len; // second arg of fn, buf_len
    ctx->esp = esp;
    ctx->eip = fn;
}
