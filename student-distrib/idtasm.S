
#define ASM

#include "idt.h"

/*
MACRO: Assembly wrapper for IRQ handlers
DESCRIPTION: It defines all the interrupt handler entry points.
INPUT: name - the name of the handler
       irqnum - the IRQ number
RETURNS: none
*/
#define IRQ_MACRO(name, irqnum) \
        . = pic_handler_start + irqnum * IDT_HANDLER_SIZE ;\
                                              ;\
        name:                            ;\
                pushl $0 /* empty error code */ ;\
                pushl %edi                      ;\
                pushl %esi                      ;\
                pushl %ebp                      ;\
                pushl %edx                      ;\
                pushl %ecx                      ;\
                pushl %ebx                      ;\
                pushl %eax                      ;\
                pushw %gs                       ;\
                pushw %fs                       ;\
                pushw %es                       ;\
                pushw %ds                       ;\
                /* copy kernel ss to ds, es, etc */ ;\
                movw %ss, %cx                   ;\
                movw %cx, %ds                   ;\
                movw %cx, %es                   ;\
                movw %cx, %fs                   ;\
                movw %cx, %gs                   ;\
                /* pushes a pointer to that pushed %ds value */ ;\
                pushl %esp                      ;\
                pushl $irqnum                 ;\
                call irq_handler                  ;\
                addl $4, %esp                 ;\
                call pop_iret_context


/*
MACRO: Assembly wrapper for exception calls 
DESCRIPTION: It defines all the exception handler entry points.
INPUT: name - the name of the handler
       vector - the vector number
RETURNS: none
*/
#define EXCEP_MACRO(name, vector) \
        . = except_handler_start + vector * IDT_HANDLER_SIZE ;\
                                              ;\
        name:                            ;\
                pushl $0 /* empty error code */ ;\
                pushl %edi                      ;\
                pushl %esi                      ;\
                pushl %ebp                      ;\
                pushl %edx                      ;\
                pushl %ecx                      ;\
                pushl %ebx                      ;\
                pushl %eax                      ;\
                pushw %gs                       ;\
                pushw %fs                       ;\
                pushw %es                       ;\
                pushw %ds                       ;\
                /* copy kernel ss to ds, es, etc */ ;\
                movw %ss, %cx                   ;\
                movw %cx, %ds                   ;\
                movw %cx, %es                   ;\
                movw %cx, %fs                   ;\
                movw %cx, %gs                   ;\
                /* pushes a pointer to that pushed %ds value */ ;\
                pushl %esp                      ;\
                pushl $vector                 ;\
                call exception_handler_all    ;\
                addl $4, %esp                 ;\
                call pop_iret_context
/*
* TODO: check excep_macro_error docs if correct.
*/      

/*
MACRO: Assembly wrapper for exception calls that push an error code
DESCRIPTION: It defines all the exception handler entry points that push an error code.
INPUT: name - the name of the handler
       vector - the vector number
RETURNS:
*/
#define EXCEP_MACRO_ERROR(name, vector) \
        . = except_handler_start + vector * IDT_HANDLER_SIZE ;\
                                              ;\
        name:                            ;\
                pushl %edi                      ;\
                pushl %esi                      ;\
                pushl %ebp                      ;\
                pushl %edx                      ;\
                pushl %ecx                      ;\
                pushl %ebx                      ;\
                pushl %eax                      ;\
                pushw %gs                       ;\
                pushw %fs                       ;\
                pushw %es                       ;\
                pushw %ds                       ;\
                /* copy kernel ss to ds, es, etc */ ;\
                movw %ss, %cx                   ;\
                movw %cx, %ds                   ;\
                movw %cx, %es                   ;\
                movw %cx, %fs                   ;\
                movw %cx, %gs                   ;\
                /* pushes a pointer to that pushed %ds value */ ;\
                pushl %esp                      ;\
                pushl $vector                 ;\
                call exception_handler_all    ;\
                addl $4, %esp                   ;\
                call pop_iret_context

.globl except_handler_start
except_handler_start:
EXCEP_MACRO(Div_by_zero_excep, 0);
EXCEP_MACRO(Debug_excep, 1);
EXCEP_MACRO(nmi_int_excep, 2);
EXCEP_MACRO(breakpoint_excep, 3);
EXCEP_MACRO(overflow_excep, 4);
EXCEP_MACRO(bound_range_exceed_excep, 5);
EXCEP_MACRO(invalid_op_excep, 6);
EXCEP_MACRO(dev_not_available_excep, 7);
EXCEP_MACRO_ERROR(double_fault_excep, 8);
EXCEP_MACRO(seg_overrrun_excep, 9);
EXCEP_MACRO_ERROR(invalid_tss_excep, 10);
EXCEP_MACRO_ERROR(seg_not_present_excep, 11);
EXCEP_MACRO_ERROR(stack_segfault_excep, 12);
EXCEP_MACRO_ERROR(gen_protect_excep, 13);
EXCEP_MACRO_ERROR(page_fault_excep, 14);
/*15 is reserved */
EXCEP_MACRO(reserved_15_excep, 15);
EXCEP_MACRO(x87_fpu_excep, 16);
EXCEP_MACRO_ERROR(align_excep, 17);
EXCEP_MACRO(machine_excep, 18);
EXCEP_MACRO(simd_excep, 19);
/* make sure we have IDT_NUM_EXCEP entries */
. = except_handler_start + IDT_NUM_EXCEP * IDT_HANDLER_SIZE

/* mappings based on the IBM PS/2 Model 80 Technical Reference manual */
.globl pic_handler_start
pic_handler_start:
IRQ_MACRO(timer_int, 0);
IRQ_MACRO(keyboard_int, 1);
IRQ_MACRO(secondary_pic_int, 2);
IRQ_MACRO(serial_alt_int, 3);
IRQ_MACRO(serial_primary_int, 4);
IRQ_MACRO(reserved_5_int, 5);
IRQ_MACRO(diskette_int, 6);
/* 7 also caused by primary spurious interrupts */
IRQ_MACRO(parallel_int, 7);
IRQ_MACRO(rtc_int, 8);
IRQ_MACRO(redir_cascade_int, 9);
IRQ_MACRO(reserved_10_int, 10);
IRQ_MACRO(reserved_11_int, 11);
IRQ_MACRO(mouse_int, 12);
IRQ_MACRO(math_coproc_excep, 13);
IRQ_MACRO(fixed_disk_int, 14);
/* 15 also caused by secondary spurious interrupts */
IRQ_MACRO(reserved_15_int, 15);
/* make sure we have IDT_NUM_EXCEP entries */
. = pic_handler_start + IDT_NUM_PIC_IRQ * IDT_HANDLER_SIZE

/* we definitely will have to modify this one later */
.globl syscall_int
syscall_int:
        /* push blank error code */
        pushl $0
        pushl %edi
        pushl %esi
        pushl %ebp
        pushl %edx
        pushl %ecx
        pushl %ebx
        pushl %eax
        pushw %gs
        pushw %fs
        pushw %es
        pushw %ds
        // copy kernel ss to ds, es, etc
        movw %ss, %cx
        movw %cx, %ds
        movw %cx, %es
        movw %cx, %fs
        movw %cx, %gs
        // pushes a pointer to that pushed %ds value
        pushl %esp
        call syscall_handler
        // reuse context pointer arg, since it's still on the stack
        call pop_iret_context

/* pop_iret_context - C calling convention
 * Takes in a pointer to a iret_context_base_t or iret_context_user_t and sets the
 * processor to match that state by popping the values off one by one.
 * Inputs: Arg 0 - iret_context - The given pointer to read process state from.
 * Outputs / Return value: never returns
 * Side effects: Loses the current execution context, changes priveledge levels. */
.globl pop_iret_context
pop_iret_context:
        // load the pointer to the context (offset 4 -> arg 1) into the stack pointer
        movl 4(%esp), %esp
        popw %ds
        popw %es
        popw %fs
        popw %gs
        popl %eax
        popl %ebx
        popl %ecx
        popl %edx
        popl %ebp
        popl %esi
        popl %edi
        /* pop error code */
        addl $4, %esp
        iret

/*
Other notes abt this file
Macro is supposed to : save some, but not all of the processor registers on the kernel stack (according to appendix B)
But saving everything for now - asked TA.
*/
