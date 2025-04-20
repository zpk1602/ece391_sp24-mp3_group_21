#ifndef _IDT_H
#define _IDT_H

#define IDT_HANDLER_SIZE 48
#define IDT_NUM_EXCEP 20
#define IDT_NUM_PIC_IRQ 16

#ifndef ASM

#include "x86_desc.h"
#include "keyboard.h"
#include "lib.h"
#include "rtc.h"

/* iret_context_base_t
 * Struct containing the coontents saved on the stack during an interrupt. Contains
 * all processor state needed to safely resume execution. */
typedef struct iret_context_base_t iret_context_base_t;
struct __attribute__((packed)) iret_context_base_t {
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t error_code;
    uint32_t eip;
    uint16_t cs;
    uint16_t cs_pad;
    uint32_t eflags;
};

/* iret_context_user_t
 * Similar to iret_context_base_t is that this struct contains the processor state needed
 * to resume, but this time also containing the values x86 pushes in the case of a priveledge
 * level change. */
typedef struct iret_context_user_t iret_context_user_t;
struct __attribute__((packed)) iret_context_user_t {
    iret_context_base_t base;
    uint32_t esp;
    uint16_t ss;
    uint16_t ss_pad;
};

void init_idt_table();
void exception_handler_all(uint32_t vect, iret_context_base_t *context);
void syscall_handler(iret_context_base_t *context);
void irq_handler(uint32_t irq, iret_context_base_t *context);
/* pop_iret_context never returns, and does not save the current CPU state at all */
void pop_iret_context(iret_context_base_t *context);

// exception lookup table initialization
// 20 entries 0-19
extern char* except_lookup[20];
extern uint8_t except_handler_start[IDT_NUM_EXCEP][IDT_HANDLER_SIZE];
extern uint8_t pic_handler_start[IDT_NUM_PIC_IRQ][IDT_HANDLER_SIZE];
extern void syscall_int;

#define IRQ_HANDLED 1
#define IRQ_UNHANDLED 0

/* irq_handler_t
 * Inputs: the number of the IRQ, 0-15
 * Return value: 1 if it handled the interrupt and sent eoi, 0 otherwise */
typedef int (*irq_handler_t)(uint32_t irq);
typedef struct irq_handler_node_t {
    irq_handler_t handler;
    struct irq_handler_node_t *next;
} irq_handler_node_t;
#define IRQ_HANDLER_NODE_INIT {.handler = NULL, .next = NULL}

/* node should be allocated in the driver using eg a static variable.
 * this adds the node to the linked list for the given irq */
void irq_register_handler(uint32_t irq, irq_handler_node_t *node);


#endif /* ASM */
#endif /* _IDT_H */

