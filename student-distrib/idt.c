#include "idt.h"
#include "lib.h"
#include "x86_desc.h"
#include "process.h"
#include "syscall.h"


/*
* exception_lookup (ARRAY)
* DESCRIPTION: This is an array of strings that contains the names of all the exceptions and interrupts that can occur.
*              When an exception or hardware interrupt is
*              detected, the processor switches into privilege level 0 (kernel mode), saves some, but not all of the processor registers
*              on the kernel stack (see Appendix E for more details), and jumps to the function address specified in the entry.
*/

char* except_lookup[IDT_NUM_EXCEP] = {
    "0. Divide by zero Exception",
    "1. Debug exception - RESERVED BY INTEL",
    "2. Non - Maskable Interrupt (NMI)",
    "3. Breakpoint Exception",
    "4. Overflow Exception",
    "5. BOUND Range Exceeded Exception",
    "6. Invalid Opcode Exception",
    "7. Device Not Available Exception",
    "8. Double Fault Exception ",
    "9. Coprocessor Segment Overrun Exception",
    "10. Invalid TSS Exception",
    "11. Segment Not Present Exception",
    "12. TSack-Segment Fault Exception",
    "13. General Protection Exception",
    "14. Page Fault Exception",
    "15. RESERVED BY INTEL",
    "16. x87 FPU Floating-Point Error Exception",
    "17. Alignment Check Exception",
    "18. Machine Check Exception",
    "19. SIMD Floating-Point Exception",
};
// wrapper macro defined in idtasm.S

/*
* FUNCTION: exception_handler_all
* DESCRIPTION: This function handler handles all exceptions and interrupts. It is called specifically when 
*              an exception or interrupt is detected.
*              It then prints out the exception or interrupt that occurred and then halts the processor.
* INPUT:  vect - the index into the IDT and it identifies the type of exception or interrupt that occurred
* RETURNS: void
*/
void exception_handler_all(uint32_t vect, iret_context_base_t *context){
    //Once an exception has occurred and the processor is executing the handler, your OS should not allow any further
    // interrupts to occur (MP3 CP1 addendum)
    // cli();
    if(vect >= IDT_NUM_EXCEP)
        panic_msg("weird! exception_handler_all called with out "
                "of bounds vector index %d!", vect);
    if(context->cs == USER_CS) {
        /* Only kill user process if exception happened in user space, otherwise
         * there is no guarantee that the kernel data structure invariants are
         * held. If the exception happened in kernel space, the best we can do is panic. */
        kill_curr_process(EXCEPTION_STATUS);
    } else panic_msg("cpu exception in kernel mode! %s", except_lookup[vect]);
    /* Note: We never run past this comment, the above branches both never return. */
    
    // CHECK : If order of these matters later. I think we should be good because we technically don't need interrupts to resume 
    // But processor MIGHT deadlock??
    while(1) asm volatile("hlt"); // Apparently this is what they mean by 'blue screen' for now
    /* don't sti in handler, iret does it for us when it restores eflags */
    // sti();
}


// 0x80
//TODO: edit documentaion when more needs to be added.
/*
* FUNCTION: syscall_handler
* DESCRIPTION: Prints out a message when a syscall is called.       
* INPUT: none
* RETURNS: void
*/
void syscall_handler(iret_context_base_t *context){
    // printf("SYSCALL HERE!!!\n");
    uint32_t sysnum = context->eax;
    int32_t ret_val;
    if(sysnum == 0 || sysnum > NUM_SYSCALLS || !syscall_tbl[sysnum-1]) {
        ret_val = -1;
    } else {
        int32_t arg1 = *(int32_t*)&context->ebx;
        int32_t arg2 = *(int32_t*)&context->ecx;
        int32_t arg3 = *(int32_t*)&context->edx;
        ret_val = syscall_tbl[sysnum-1](arg1, arg2, arg3);
    }
    *(int32_t*)&context->eax = ret_val;
}

static irq_handler_node_t *irq_handlers[IDT_NUM_PIC_IRQ];

/*
* FUNCTION: irq_register_handler
* DESCRIPTION: This function is used to register a handler for a given IRQ.
*              It takes in the irq number and a pointer to the handler node.
*              and adds the handler to the list of handlers for the given irq.
*              
* INPUT: irq - the irq number. (An index into the IDT)
*        node - a pointer to a handler node to add to the front of the list.
* RETURNS: void
*/
void irq_register_handler(uint32_t irq, irq_handler_node_t *node) {
    if(irq >= IDT_NUM_PIC_IRQ) {
        panic_msg("irq num %d outside of IRQ range!", irq);
    }
    if(node == NULL) panic_msg("NULL irq handler node!");
    if(node->handler == NULL) panic_msg("NULL irq handler!");
    if(node->next != NULL) panic_msg("node 0x%#x already points at node 0x%#x!"
                                     " should be null", node, node->next);

    uint32_t flags;
    cli_and_save(flags);

    /* check to see if node is already in the list; adding it would cause a linked loop */
    irq_handler_node_t *curr = irq_handlers[irq];
    while(curr != NULL) {
        if(curr == node) panic_msg("node 0x%#x already in linked list!", node);
        curr = curr->next;
    }

    /* insert at head */
    node->next = irq_handlers[irq];
    irq_handlers[irq] = node;
    restore_flags(flags);
}

/*
* FUNCTION: irq_handler
* DESCRIPTION: This function is the main handler for IRQs
*              It is called when an IRQ occurs. The function checks if the IRQ number is within the valid range. 
*              If it is, it calls all the handlers registered for this IRQ until one of them handles the IRQ. 
*              If no handler is able to handle the IRQ, it prints an error message and halts the system.
* INPUT: irq - an index into the IDT
* RETURNS: void
*/
void irq_handler(uint32_t irq, iret_context_base_t *context) {
    if(irq >= IDT_NUM_PIC_IRQ) {
        panic_msg("huh? main irq handler called with irq num %d outside range?", irq);
    }
    irq_handler_node_t *curr = irq_handlers[irq];
    int handled = 0;
    if(curr == NULL) panic_msg("no handlers registered for enabled irq num %d", irq);
    while(curr != NULL) {
        handled = handled || curr->handler(irq);
        curr = curr->next;
    }
    if(!handled) panic_msg("unhandled enabled irq num %d!", irq);
}

/*
* FUNCTION: init_idt_table
* DESCRIPTION: This function initializes the Interrupt Descriptor Table (IDT). 
*              The IDT is used by the processor to determine the correct response to interrupts and exceptions.
* INPUTS: none
* RETURNS: void
*/
void init_idt_table() {

    // Set entry DPL bit to 0 for switch to kernel mode
    // need to set all the bits for each of the idt entried here before setting it to a handler 

    // have a for loop for each of the 0-19 IDT and init each vector num in the table to a struct
    // change the bits 
    int j;
    idt_desc_t idt_ent;
    idt_ent.val[0] = 0; idt_ent.val[1] = 0;
    // REF IA32 vol 3 5.11 IDT Interrupt gate descriptor for bitwise info
    // Setting struct values for each entry in idt (struct def in x86_desc.h)
    // offset_15_00 and offset_31_16 are set by SET_IDT_ENTRY later. 

    idt_ent.seg_selector = KERNEL_CS; //all exceptions and interrupts happen in kernel space
    //Setting the reserve bits - 01110 (pls recheck later, most probably correct for now)
    
    idt_ent.reserved0 = 0;
    idt_ent.reserved1 = 1;
    idt_ent.reserved2 = 1;
    idt_ent.reserved3 = 0;
    idt_ent.reserved4 = 0;

    // //if syscall set DPL bit to 3 so that it is accessible from user space via the int instruction (appendix B MP3 doc)
    // idt[i].dpl = 3;

    //For exceptions and interrupts set DPL bit to 0 to prevent user code from accessing these routines (appendix B MP3 doc)
    idt_ent.dpl = 0;

    // set 0x80 dpl to 3 later

    idt_ent.present = 1; //set to 1 by default, if not will cause an error 
    idt_ent.size = 1; //set to 1 for 32 bit gate descriptors

    // 0 - 19 vectors in IDT are exceptions (defined in IA32 Table 5-1 ch5.3)
    for ( j = 0; j < IDT_NUM_EXCEP; j++) {
        // Entry 15 is reserved by intel, still set it, in case it goes off
        SET_IDT_ENTRY(idt_ent, except_handler_start + j);
        idt[j] = idt_ent;
    }

    // int 0x20 - 0x2F map to IRQ's 0 - 15
    for ( j = 0; j < 16; j++) {
        // no need to skip entries, PIC will mask ones we don't use
        SET_IDT_ENTRY(idt_ent, pic_handler_start + j);
        idt[0x20 + j] = idt_ent;
    }


    // had this for a different way of implementation with separate exception handlers
    // wrote more efficient code using lookup table but 
    // keeping it here just in case we need to go back to doing this

    // SET_IDT_ENTRY(idt[0], Div_by_zero_excep);
    // SET_IDT_ENTRY(idt[1], Debug_excep);
    // SET_IDT_ENTRY(idt[2], nmi_int_excep);
    // SET_IDT_ENTRY(idt[3], breakpoint_excep);
    // SET_IDT_ENTRY(idt[4], overflow_excep);
    // SET_IDT_ENTRY(idt[5], bound_range_exceed_excep);
    // SET_IDT_ENTRY(idt[6], invalid_op_excep);
    // SET_IDT_ENTRY(idt[7], dev_not_available_excep);
    // SET_IDT_ENTRY(idt[8], double_fault_excep);
    // SET_IDT_ENTRY(idt[9],  seg_overrrun_exce );
    // SET_IDT_ENTRY(idt[10], invalid_tss_excep);
    // SET_IDT_ENTRY(idt[11], seg_not_present_excep);
    // SET_IDT_ENTRY(idt[12], stack_segfault_excep);
    // SET_IDT_ENTRY(idt[13], gen_protect_excep);
    // SET_IDT_ENTRY(idt[14], page_fault_excep);
    // SET_IDT_ENTRY(idt[16], x87_fpu_excep);
    // SET_IDT_ENTRY(idt[17], align_excep);
    // SET_IDT_ENTRY(idt[18], machine_excep);
    // SET_IDT_ENTRY(idt[19], simd_excep);


    // 0x21 and 0x28 are the resp vectors set by intel
    // SET_IDT_ENTRY(idt[0x21], &keyboard_int);
    // SET_IDT_ENTRY(idt[0x28], &rtc_int);
    

    // 0x80 to be defined for syscalls based on MP3 doc
    SET_IDT_ENTRY(idt_ent, &syscall_int);
    idt[0x80] = idt_ent;
    idt[0x80].dpl = 3;
    idt[0x80].reserved3 = 1; /* set syscall to be trap gate, don't disable interrupts */

}
 

