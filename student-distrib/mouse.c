/* mouse.c - mouse driver implementation */

#include "types.h"
#include "mouse.h"
#include "lib.h"
#include "idt.h"
#include "i8259.h"

#define PS2_CMD_PORT 0x64 // PS2 IO port
#define PS2_DATA_PORT 0x60 // PS2 IO port
#define PS2_WRITE_CFG 0x60 // PS2 ctrl cmd
#define PS2_READ_CFG 0x20 // PS2 ctrl cmd
#define PS2_OUT_BUF_FULL 0x1 // PS2 status flag
#define PS2_IN_BUF_FULL 0x2 // PS2 status flag
#define PS2_DIS_KBD 0xAD // PS2 ctrl cmd
#define PS2_DIS_MOUSE 0xA7 // PS2 ctrl cmd
#define PS2_WRITE_MOUSE 0xD4 // PS2 ctrl cmd
#define PS2_MOUSE_DIS_FLAG 0x20 // PS2 ctrl reg flag
#define PS2_KBD_DIS_FLAG 0x10 // PS2 ctrl reg flag
#define PS2_ENABLE_INT 0x3 // PS2 ctrl reg flags
#define MOUSE_EN_DATA_REPORT 0xF4 // PS2 mouse cmd
#define MOUSE_IRQ 12 // PIC IRQ num
#define MOUSE_PACKET_LEN 3 // length of mouse data packets we receive

static int mouse_handler(uint32_t irq);

/* Initializes the PS2 mouse driver. if there is no mouse, it should do nothing,
 * leaving the PS2 hardware in the same state as it started. (in theory) */
void mouse_init(void) {
    uint32_t flags;
    cli_and_save(flags);

    /* disable devices so they don't interfere with configuration */
    outb(PS2_DIS_KBD, PS2_CMD_PORT);
    outb(PS2_DIS_MOUSE, PS2_CMD_PORT);
    /* clear buffers */
    while(inb(PS2_CMD_PORT) & PS2_OUT_BUF_FULL) inb(PS2_DATA_PORT);
    while(inb(PS2_CMD_PORT) & PS2_IN_BUF_FULL);

    outb(PS2_READ_CFG, PS2_CMD_PORT);
    while(!(inb(PS2_CMD_PORT) & PS2_OUT_BUF_FULL));
    uint8_t cfg_byte = inb(PS2_DATA_PORT);

    int mouse_present = !!(cfg_byte & PS2_MOUSE_DIS_FLAG);
    log_msg("mouse %sdetected", mouse_present ? "" : "NOT ");

    if(mouse_present) {
        /* re-enable mouse only */
        cfg_byte = cfg_byte & ~PS2_MOUSE_DIS_FLAG;
        outb(PS2_WRITE_CFG, PS2_CMD_PORT);
        while(inb(PS2_CMD_PORT) & PS2_IN_BUF_FULL);
        outb(cfg_byte, PS2_DATA_PORT);
        while(inb(PS2_CMD_PORT) & PS2_IN_BUF_FULL);

        /* enable data reporting in mouse device (interrupts) */
        outb(PS2_WRITE_MOUSE, PS2_CMD_PORT);
        while(inb(PS2_CMD_PORT) & PS2_IN_BUF_FULL);
        outb(MOUSE_EN_DATA_REPORT, PS2_DATA_PORT);

        /* read acknowledge byte */
        while(!(inb(PS2_CMD_PORT) & PS2_OUT_BUF_FULL));
        if(inb(PS2_DATA_PORT) != 0xFA) panic_msg("didn't get mouse ack");

        /* enable keyboard and interrupts in PS2 controller */
        cfg_byte = (cfg_byte | PS2_ENABLE_INT) & ~PS2_KBD_DIS_FLAG;
        outb(PS2_WRITE_CFG, PS2_CMD_PORT);
        while(inb(PS2_CMD_PORT) & PS2_IN_BUF_FULL);
        outb(cfg_byte, PS2_DATA_PORT);
        while(inb(PS2_CMD_PORT) & PS2_IN_BUF_FULL);
        log_msg("ps2 cmd byte 0x%x", cfg_byte);

        static irq_handler_node_t mouse_node;
        mouse_node.handler = &mouse_handler;
        irq_register_handler(MOUSE_IRQ, &mouse_node);

        enable_irq(MOUSE_IRQ);
    } else {
        /* re-enable keyboard */
        cfg_byte = cfg_byte & ~PS2_KBD_DIS_FLAG;
        outb(PS2_WRITE_CFG, PS2_CMD_PORT);
        while(inb(PS2_CMD_PORT) & PS2_IN_BUF_FULL);
        outb(cfg_byte, PS2_DATA_PORT);
        while(inb(PS2_CMD_PORT) & PS2_IN_BUF_FULL);
    }

    restore_flags(flags);
}

static int mouse_pos = 0; // whether we are at byte 0, 1, or 2 of the current mouse packet
// absolute mouse coords relative to starting position, not contained in any screen coords
int32_t mouse_x = 0;
int32_t mouse_y = 0;
// first byte of packet, contains mouse button states
uint8_t mouse_buttons = 0;

/* mouse_handler - Handles a PS2 mouse interrupt, reading in a byte of data from the mouse,
 * and updates the global variables as needed. */
static int mouse_handler(uint32_t irq) {
    uint8_t data = inb(PS2_DATA_PORT);
    // log_msg("got mouse data: 0x%x", data);

    switch(mouse_pos) {
    case 0: // header: buttons, sign bits
        mouse_buttons = data;
        break;
    case 1: // x delta
        mouse_x += (int32_t) data - (((int32_t) mouse_buttons << 4) & 0x100);
        break;
    case 2: // y delta
        mouse_y += (int32_t) data - (((int32_t) mouse_buttons << 3) & 0x100);

        // log_msg("pos x: %d y: %d left: %u right: %u middle: %u",
        //     mouse_x >> 5, mouse_y >> 5,
        //     !!(mouse_buttons & MOUSE_LEFT),
        //     !!(mouse_buttons & MOUSE_RIGHT),
        //     !!(mouse_buttons & MOUSE_MIDDLE)
        // );
        break;
    }
    mouse_pos = (mouse_pos+1) % MOUSE_PACKET_LEN;

    send_eoi(MOUSE_IRQ);
    return 1; // serviced interrupt
}
