/* i8259.h - Defines used in interactions with the 8259 interrupt
 * controller
 * vim:ts=4 noexpandtab
 */

#ifndef _I8259_H
#define _I8259_H

#include "types.h"

/* COMMAND Ports that each PIC sits on */
/* 
Command port is to send commands to PIC
like init - ICW, setting op mode, EOI, configuring interrupts
*/
#define CMD_MASTER_8259_PORT    0x20
#define CMD_SLAVE_8259_PORT     0xA0


// DATA Pots for each PIC - we defined this
/*
Data port is used to send/receive data regarding PIC config
like programming or reading IMR - interrupt mask register, 
setting or reading the interrupt vector base address using ICWs during init
*/
#define DATA_MASTER_8259_PORT    0x21
#define DATA_SLAVE_8259_PORT     0xA1

/* Initialization control words to init each PIC.
 * See the Intel manuals for details on the meaning
 * of each word */
#define ICW1                0x11 // Starts init process and configures basic settings of PIC
#define ICW2_MASTER         0x20 // Sets the base address for interrupt vectors for PIC
#define ICW2_SLAVE          0x28 // ICW2 - IMP FOR IDT
#define ICW3_MASTER         0x04 // Configures the cascade between PICs 
#define ICW3_SLAVE          0x02
#define ICW4                0x01  // supposed to have additional func but we don't care 


/* End-of-interrupt byte.  This gets OR'd with
 * the interrupt number and sent out to the PIC
 * to declare the interrupt finished */
#define EOI                 0x60

/* Externally-visible functions */

/* Initialize both PICs */
void i8259_init(void);

/* Enable (unmask) the specified IRQ */
void enable_irq(uint32_t irq_num);


/* Disable (mask) the specified IRQ */
void disable_irq(uint32_t irq_num);

/* Send end-of-interrupt signal for the specified IRQ */
void send_eoi(uint32_t irq_num);
/* Notes that might be useful later on:
1. Interrupts by slave delivered to master by IRQ2
 and only delivered to the processor when no other higherpriority interrupts (IR0 and IR1 on the primary PIC) are in service.

2. */
#endif /* _I8259_H */

