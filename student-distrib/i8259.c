/* i8259.c - Functions to interact with the 8259 interrupt controller
 * vim:ts=4 noexpandtab
 */

#include "i8259.h"
#include "lib.h"

/* Interrupt masks to determine which interrupts are enabled and disabled */
// we have access to 15 IRQs because we lose one port when cascading


uint8_t master_mask; /* IRQs 0-7  */
uint8_t slave_mask;  /* IRQs 8-15 */

// Code ref: https://wiki.osdev.org/PIC

// Need to use the ICW thing??
/* Initialize the 8259 PIC */
/*
* FUNCTION: i8259_init
* DESCRIPTION: This function initializes the 8259 PIC.
*              It sends the ICW1-ICW4 initialization control words to the PICs.
*              It also masks all interrupts.
* INPUTS: none
* OUTPUTS: none
*/
// basically adapt from linuxs i8259.c code
void i8259_init(void) {
    // entire initialization code is supposed to be one critical section
    uint32_t flags;
    cli_and_save(flags);
    uint32_t i;
    // REF datasheet for ICW bit codes
    outb(ICW1,CMD_MASTER_8259_PORT);
    outb(ICW1,CMD_SLAVE_8259_PORT);
    outb(ICW2_MASTER, DATA_MASTER_8259_PORT);
    outb(ICW2_SLAVE, DATA_SLAVE_8259_PORT);
    outb(ICW3_MASTER, DATA_MASTER_8259_PORT);
    outb(ICW3_SLAVE, DATA_SLAVE_8259_PORT);
    
    outb(ICW4, DATA_MASTER_8259_PORT);
    outb(ICW4, DATA_SLAVE_8259_PORT);
    
    for ( i = 0; i <=15; i++){
        // mask interrupts for all signals
        // except IRQ2 on master for proper cascading 
        if (i != 2) disable_irq(i);
        else enable_irq(i);
    }

    restore_flags(flags);
}



/* 
PIC has a register called IMR - 
interrupt mask register which is the bitmap of all the lines going into the PIC
*/

/*
FUNCTION: void enable_irq(uint32_t irq_num);
DESCRIPTION: Enable (unmask) the specified IRQ
INPUT:  masked IRQ number 
RETURNS: void 
*/
void enable_irq(uint32_t irq_num) {
    uint16_t port;
    uint8_t value;

    if (irq_num < 8) {
        // If the IRQ is managed by the master PIC
        port = DATA_MASTER_8259_PORT;
        master_mask &= ~(1 << irq_num); // Clear the bit for irq_num, enabling it
        value = master_mask;
    } else if (irq_num < 16) {
        // If the IRQ is managed by the slave PIC
        port = DATA_SLAVE_8259_PORT;
        slave_mask &= ~(1 << (irq_num - 8)); // Clear the bit for irq_num and adjust for slave offset
        value = slave_mask;
    } else {
        // if invalid IRQnum 
        panic_msg("irq_num %d outside of valid range!", irq_num);
    }

    // Write the updated mask to the PIC
    outb(value, port);
}
/*
FUNCTION: void disable_irq(uint32_t irq_num);
DESCRIPTION: Disable (mask) the specified IRQ
INPUT:  unmasked IRQ number 
RETURNS: void 
*/

void disable_irq(uint32_t irq_num) {
    uint16_t port;
    uint8_t value;

    if (irq_num < 8) {
        // If the IRQ is managed by the master PIC
        port = DATA_MASTER_8259_PORT;
        master_mask |= (1 << irq_num); // Set the bit for irq_num, disabling it
        value = master_mask;
    } else if (irq_num < 16) {
        // The IRQ is managed by the slave PIC
        port = DATA_SLAVE_8259_PORT;
        slave_mask |= (1 << (irq_num - 8)); // Set the bit for irq_num, adjusting for slave offset, disabling it
        value = slave_mask;
    } else {
        // If invalid
        panic_msg("irq_num %d outside of valid range!", irq_num);
    }

    // Write the updated mask to the PIC
    outb(value, port);
}

/* Send end-of-interrupt signal for the specified IRQ */
/*
* FUNCTION: send_eoi
* DESCRIPTION: This function sends an End-Of-Interrupt (EOI) signal for the specified IRQ. 
*              The EOI signal is used to tell the PIC that it has finished 
*              servicing an interrupt. If the IRQ is serviced by the master PIC, it sends an EOI signal to the master PIC. 
*              If the IRQ is serviced by the slave PIC, it sends an EOI signal to both the slave and the master PIC.
* INPUT: irq_num: The IRQ number. This is an index into the IDT and identifies the type of interrupt.
* RETURNS: void
*/
void send_eoi(uint32_t irq_num) {
    if (irq_num < 8) {
        // If the IRQ is serviced by the master PIC
        outb(EOI | irq_num, CMD_MASTER_8259_PORT);
    } else if (irq_num < 16) {
        // If the IRQ is serviced by the slave PIC
        // Send EOI to both the slave and the master PIC
        outb(EOI | (irq_num - 8), CMD_SLAVE_8259_PORT); // Correct the IRQ number for the slave PIC
        outb(EOI | 2, CMD_MASTER_8259_PORT); // IRQ2 is used to connect the slave to the master
    } else {
        panic_msg("irq_num %d outside of valid range!", irq_num);
    }
}
