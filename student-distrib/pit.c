/*
* pit.c - Implements the PIT (Programmable Interval Timer) functions.
*/

#include "pit.h"
#include "i8259.h"
#include "lib.h"
#include "process.h"
#include "syscall.h"
#include "gui.h"

volatile int enable_pit_test = 0;
static int pit_handler(uint32_t irq);

/* pit_init
 * Initializes the PIT to a default frequency.
 * Inputs: none
 * Outputs: none
 * Return value: none
 * Side effects: Enables the PIT IRQ line.
 */
void pit_init() {
    uint32_t flags;
    cli_and_save(flags);
    /* set the PIT to a default frequency */
    outb(0x36, PIT_DATA_PORT); // 0011 0110 - channel 0, lobyte/hibyte, rate generator
    outb(PIT_DEFAULT_TIME & 0xFF, PIT_DATA_PORT);
    outb((PIT_DEFAULT_TIME >> 8) & 0xFF, PIT_DATA_PORT);

    enable_irq(PIT_IRQ);
    
    static irq_handler_node_t pit_handler_node = IRQ_HANDLER_NODE_INIT;
    pit_handler_node.handler = &pit_handler;
    irq_register_handler(PIT_IRQ, &pit_handler_node);

    restore_flags(flags);
}

/* pit_handler
 * Handles the PIT interrupt.
 * Inputs: irq - The IRQ number that was triggered.
 * Outputs: none
 * Return value: none
 * Side effects: Calls the scheduler to switch tasks.
 */
int pit_handler(uint32_t irq) {
    /* call the scheduler to switch tasks */
    if(enable_pit_test) printf("PIT interrupt\n");
    send_eoi(PIT_IRQ);

    do_render(); // in gui.c

    pcb_t *curr_pcb = get_current_pcb();

    // this check ensures we don't invoke the scheduler before processes are running
    if(curr_pcb->present) {
        do_schedule(0); // switch, don't jump, so pass in 0 as arg
    }

    return 1; /* handled the irq */
}


/* pit_setrate
 * Sets the PIT to a specified frequency.
 * Inputs: rate - The frequency to set the PIT to.
 * Outputs: none
 * Return value: none
 * Side effects: Changes the PIT frequency.
 */
// void pit_setrate(uint32_t rate) {
//     uint16_t divisor = PIT_FREQ / rate;
//     outb(0x36, PIT_CMD_PORT); // 0011 0110 - channel 0, lobyte/hibyte, rate generator
//     outb(divisor & 0xFF, PIT_DATA_PORT);
//     outb((divisor >> 8) & 0xFF, PIT_DATA_PORT);
// }
