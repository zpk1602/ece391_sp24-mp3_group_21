/* rtc.c -- Real Time Clock driver implementation */

#include "x86_desc.h"
#include "lib.h"
#include "rtc.h"
#include "i8259.h"
#include "idt.h"
#include "fs.h"
#include "fd.h"

#define RTC_BASE_RATE 1024

static int rtc_handler(uint32_t irq);
int enable_rtc_test = 0;

/*
We use a doubly linked list for keeping track of the open RTC file descriptors,
since overall there are 6*8 such file descriptors, so looping through all of them would
be a big waste of time since only a few will ever be RTC fd's. We can't really waste time
in something that runs 1024 times a second (rtc_handler), so instead, we maintain a
doubly-linked list of the rtc file descriptor driver_data fields.
*/

typedef struct rtc_driver_data_t rtc_driver_data_t;
struct rtc_driver_data_t {
    rtc_driver_data_t *next;
    rtc_driver_data_t *prev;
    /* only when the global rtc counter and'ed with this mask is zero do we fire
     * an interrupt for this file descriptor. so a mask of zero will fire always,
     * while a mask of 511 will fire twice a second. */
    uint32_t mask;
    // flag for whether we had an interrupt fired, 1 for yes, 0 for no
    uint32_t fired;
};

rtc_driver_data_t *rtc_driver_data_head = NULL;
uint32_t rtc_driver_counter = 0;

// TODO: add virtualization of RTC (multiple terminals) (see appendix B of manual for details)
// TODO: change 

/* void rtc_init()
 * Initializes the Real Time Clock, setting its frequency, enabling its IRQ line in the
 * PIC, and registering its interrupt handler with the IRQ system
 * Inputs / Outputs / Return value: none
 * Side effects: Sends data to the RTC, modifies the IRQ linked lists, sends data to the PIC
 */
void rtc_init(){
    // check to make sure rtc_driver_data_t can fit inside the driver_data field of fd_info_t
    if(sizeof(rtc_driver_data_t) > sizeof(((fd_info_t*)0)->driver_data))
        panic_msg("rtc_driver_data_t too big to fit inside driver_data");
    // check alignment as well
    /* see https://gcc.gnu.org/onlinedocs/gcc-10.5.0/gcc/Alignment.html for how
     * this operator works. C11 went on to standardize it, but since we're old school,
     * we're stuck with the GNU specific version. */
    if(__alignof__(rtc_driver_data_t) > __alignof__(((fd_info_t*)0)->driver_data))
        panic_msg("rtc_driver_data_t has greater alignment requirement than driver_data");

      // https://wiki.osdev.org/RTC
      // link has code we can adapt
      // we have outputb = outb in lib.h
    uint32_t flags;
    cli_and_save(flags);

    /* set time base to max (bits 6-4 of data) and intr freq to 2Hz (bits 3-0 of data) */
    outb(RTC_MASK_NMI | RTC_REG_A, RTC_ADDR);
    outb(0x06, RTC_DATA); // TODO: change it to 1 khz (0x06) when we virtualize
    /* enable periodic interrupts bit, all other bits can/should be zero since we don't
     * use the date/clock functionality of the RTC */
    outb(RTC_MASK_NMI | RTC_REG_B, RTC_ADDR);
    outb(0x40, RTC_DATA);
    enable_irq(RTC_IRQ);

    static irq_handler_node_t rtc_handler_node = IRQ_HANDLER_NODE_INIT;
    rtc_handler_node.handler = &rtc_handler;
    irq_register_handler(RTC_IRQ, &rtc_handler_node);

    restore_flags(flags);
}
// when you recevie an rtc interrrupt print smth
// call test_interrupts??


/* int32_t rtc_setrate(uint32_t rate)
 * Sets the rate of the RTC to the given value, which must be a power of 2 between 2 and 1024
 * Inputs: rate - the rate to set the RTC to
 * Outputs: none
 * Return value: 1 if the rate was set successfully, 0 otherwise
 * Side effects: Writes to the RTC
 */
int32_t rtc_setrate(uint32_t rate) {
    uint32_t flags;
    cli_and_save(flags);
    if (((rate - 1) & rate) != 0 || rate > 1024 || rate < 2) { // checks if rate is power of 2 and in range 2-1024
        restore_flags(flags);
        return 0;
    } 
    // Calculate log2(rate) using bitwise operations
    uint32_t rtc_rate = 0;
    uint32_t value = rate;
    while (value >>= 1) {
        rtc_rate++;
    }
    rtc_rate = 16 - rtc_rate; // rate should be 16 - log 2 of rate
    outb(RTC_MASK_NMI | RTC_REG_A, RTC_ADDR); // writes nmi to register A and RTC_ADDR
    uint8_t prev = inb(RTC_DATA); // reads from RTC_DATA
    outb(RTC_MASK_NMI | RTC_REG_A, RTC_ADDR);
    outb((prev & 0xF0) | (rtc_rate & 0x0F), RTC_DATA); // writes to RTC_DATA while preserving the upper 4 bits 
    restore_flags(flags);
    return 1;
}

/* int rtc_handler(uint32_t irq)
 * Handles an RTC periodic interrupt, currently just calling a test function
 * Inputs / Outputs / Return value: See irq_handler_t in idt.h
 * Side effects: Sends EOI to PIC and to RTC
 */
static int rtc_handler(uint32_t irq) {
    // static int funny = 0;
    //make sure to call send_eoi for PIC

    if(enable_rtc_test) test_interrupts();
    // if(funny++ > 15) asm volatile("ud2");

    rtc_driver_data_t *curr = rtc_driver_data_head;
    while(curr) {
        if((rtc_driver_counter & curr->mask) == 0) {
            curr->fired = 1;
        }
        curr = curr->next;
    }

    ++rtc_driver_counter;

    /* read reg C to signal end of interrupt to RTC */
    outb(0x8C, RTC_ADDR);
    inb(RTC_DATA);
    send_eoi(RTC_IRQ);
    /* don't sti yet, can cause unbounded stack lol */
    return IRQ_HANDLED; /* handled the irq */
}

static inline uint32_t freq_to_mask(uint32_t freq) {
    return (RTC_BASE_RATE / freq) - 1;
}

/*
* rtc_open
* DESCRIPTION: Opens the RTC file'
* INPUTS: fd_info - file descriptor info struct'
*         filename - name of the file to open
* OUTPUTS: none
* RETURNS: 0 on success, -1 on failure
* SIDE EFFECTS: Sets the rate of the RTC to 2 Hz
*/
int32_t rtc_open(fd_info_t *fd_info, const uint8_t *filename) {
    // set rate to 2 Hz
    // enable irq 8 here maybe? not sure?
    if(!fd_info || !filename) return -1;
    // if(!rtc_setrate(2)) return -1;
    // initialize fd info struct 
    fd_info->file_ops = &rtc_fd_driver;
    fd_info->inode = 0;
    fd_info->file_pos = 0;

    uint32_t flags;
    cli_and_save(flags);
    rtc_driver_data_t *rtc_data = (rtc_driver_data_t*) &fd_info->driver_data;
    if(rtc_driver_data_head) rtc_driver_data_head->prev = rtc_data;
    rtc_data->prev = NULL;
    rtc_data->next = rtc_driver_data_head;
    rtc_data->mask = freq_to_mask(2); // start at 2 Hz
    rtc_data->fired = 1;
    rtc_driver_data_head = rtc_data;
    restore_flags(flags);

    return 0; /*is this really it?*/
}



/*
* rtc_close
* DESCRIPTION: Closes the RTC file'
* INPUTS: fd_info - file descriptor info struct'
* OUTPUTS: none
* RETURNS: 0 on success, -1 on failure
*/
int32_t rtc_close(fd_info_t *fd_info) {
    if(!fd_info) return -1;
    uint32_t flags;
    cli_and_save(flags);
    rtc_driver_data_t *rtc_data = (rtc_driver_data_t*) &fd_info->driver_data;
    rtc_driver_data_t *old_prev = rtc_data->prev, *old_next = rtc_data->next;
    if(old_prev) old_prev->next = old_next;
    else rtc_driver_data_head = old_next;
    if(old_next) old_next->prev = old_prev;
    restore_flags(flags);
    return 0; /*TODO: redo when virturalize */

}



/*
* rtc_read
* DESCRIPTION: Reads from the RTC file'
* INPUTS: fd_info - file descriptor info struct'
*         buf - buffer to read into
*         nbytes - number of bytes to read
* OUTPUTS: none
* RETURNS: 0 on success, -1 on failure
*/
int32_t rtc_read(fd_info_t *fd_info, void *buf, int32_t nbytes) {
    if(!buf || !fd_info || nbytes < 0) return -1;
    rtc_driver_data_t *rtc_data = (rtc_driver_data_t*) &fd_info->driver_data;
    rtc_data->fired = 0;
    while(!rtc_data->fired) {
        // wait for rtc interrupt
        asm volatile("hlt");
    }
    fd_info->file_pos++;
    return 0; /*TODO:*/
}



/*
* rtc_write
* DESCRIPTION: Writes to the RTC file'
* INPUTS: fd_info - file descriptor info struct'
*         buf - buffer to write from
*         nbytes - number of bytes to write
* OUTPUTS: none
* RETURNS: number of bytes written on success, -1 on failure
*/
int32_t rtc_write (fd_info_t *fd_info, const void *buf, int32_t nbytes) {
    if(nbytes != 4 || !buf || !fd_info) return -1;

    uint32_t rate = *(uint32_t*)buf;

    if(((rate-1) & rate) != 0 || rate > 1024 || rate < 2) return -1;

    rtc_driver_data_t *rtc_data = (rtc_driver_data_t*) &fd_info->driver_data;
    rtc_data->mask = freq_to_mask(rate);

    return 4;
}



/*
* rtc_fd_driver
* DESCRIPTION: File descriptor driver for the RTC file'
*/
fd_driver_t rtc_fd_driver = {
    .open = rtc_open,
    .close = rtc_close,
    .read = rtc_read,
    .write = rtc_write,
};
