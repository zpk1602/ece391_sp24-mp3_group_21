#include "keyboard.h"

#include "i8259.h"
#include "lib.h"
#include "idt.h"
#include "terminal.h"
#include "process.h"
#include "gui.h"

static int shift_pressed = FALSE;
static int ctrl_pressed = FALSE;
static int caps_lock_active = FALSE;
static int alt_pressed = FALSE;

static int keyboard_handler(uint32_t irq);

/*
 * FUNCTION: keyboard_init
 * DESCRIPTION: Initializes the keyboard for use within the operating system. 
 *              This function enables the keyboard interrupt on the PIC,
 *              allowing the system to start receiving keyboard interrupts.
 * INPUTS: none
 * OUTPUTS: none
 * RETURNS: void
 * SIDE EFFECTS: The system will begin to respond to key presses by generating interrupts.
 */
void keyboard_init(){
    uint32_t flags;
    cli_and_save(flags);

    shift_pressed = FALSE;
    ctrl_pressed = FALSE;
    caps_lock_active = FALSE;

    static irq_handler_node_t keyboard_node;
    keyboard_node.handler = &keyboard_handler;
    irq_register_handler(KEYBOARD_IRQ, &keyboard_node);

    enable_irq(KEYBOARD_IRQ);

    restore_flags(flags);
}

/**
 * scancode_to_ascii, scancode_to_shifted_ascii, scancode_to_capslock_ascii, scancode_to_capsshift_ascii
 * 
 * These are arrays containing the mapping of keyboard scan codes to ASCII characters.
 * The mapping is based on scan code set 1.
 * 
 * The arrays are indexed by the scan code, and the value at each index
 * is the corresponding ASCII character.
 * 
 * The arrays include mappings for:
 * - Number keys (1-0)
 * - Numpad keys (1-9, 0, +, -, *, .)
 * - Lowercase letter keys (a-z)
 * - Other symbol keys (` - = [ ] \ ; ' , . / space)
 * - Enter, Tab, Control, Caps Lock keys
 *
 * The array used depends on if the user is pressing shift, caps lock, both shift and caps lock, or neither.
 */
// Mapping of scan codes to ASCII characters using scan code set 1

static char scancode_to_ascii[NUM_SCANCODES] = {
    // Number keys
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5', 
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0', 

    // // Numpad keys
    // [0x4f] = '1', [0x50] = '2', [0x51] = '3', [0x4b] = '4', [0x4c] = '5',
    // [0x4d] = '6', [0x47] = '7', [0x48] = '8', [0x49] = '9', [0x52] = '0',
    // [0x4e] = '+', [0x4A] = '-', [0x37] = '*', [0x53] = '.',
    
    // Lowercase letter keys
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't', 
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p', 
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', 
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x2C] = 'z',
    [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n',
    [0x32] = 'm', 

    // Other keys 
    [0x29] = '`', [0x0C] = '-', [0x0D] = '=', [0x1A] = '[', [0x1B] = ']',
    [0x2B] = '\\', [0x27] = ';', [0x28] = '\'', [0x33] = ',', [0x34] = '.',
    [0x35] = '/', [0x39] = ' ',

    // Other function keys
    [0x1C] = '\n',
};

static char scancode_to_shifted_ascii[NUM_SCANCODES] = {
    // Number keys in caps
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%', 
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',

    // Uppercase letter keys
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T', 
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P', 
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G', 
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x2C] = 'Z',
    [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B', [0x31] = 'N',
    [0x32] = 'M', 

    // Other keys in caps
    [0x29] = '~', [0x0C] = '_', [0x0D] = '+', [0x1A] = '{', [0x1B] = '}',
    [0x2B] = '|', [0x27] = ':', [0x28] = '"', [0x33] = '<', [0x34] = '>',
    [0x35] = '?', [0x39] = ' ',

    // Other function keys
    [0x1C] = '\n',
};

static char scancode_to_capslock_ascii[NUM_SCANCODES] = {
    // Number keys
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5', 
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',

    // Uppercase letter keys
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T', 
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P', 
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G', 
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x2C] = 'Z',
    [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B', [0x31] = 'N',
    [0x32] = 'M', 

    // Other keys
    [0x29] = '`', [0x0C] = '-', [0x0D] = '=', [0x1A] = '[', [0x1B] = ']',
    [0x2B] = '\\', [0x27] = ';', [0x28] = '\'', [0x33] = ',', [0x34] = '.',
    [0x35] = '/', [0x39] = ' ',

    // Other function keys
    [0x1C] = '\n',
};

static char scancode_to_capsshift_ascii[NUM_SCANCODES] = {
    // Number keys in caps
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%', 
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',

    // Lowercase letter keys
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't', 
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p', 
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', 
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x2C] = 'z',
    [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n',
    [0x32] = 'm', 

    // Other keys in caps
    [0x29] = '~', [0x0C] = '_', [0x0D] = '+', [0x1A] = '{', [0x1B] = '}',
    [0x2B] = '|', [0x27] = ':', [0x28] = '"', [0x33] = '<', [0x34] = '>',
    [0x35] = '?', [0x39] = ' ',

    // Other function keys
    [0x1C] = '\n',
};

/*
 * FUNCTION: keyboard_handler
 * DESCRIPTION: Handles the keyboard interrupt by reading a scancode from the keyboard
 *              data port and translating it to an ASCII character if it is a valid
 *              scancode. It then outputs the character to the screen.
 * INPUTS: irq -- The irq number on the PIC, ranging from 0 to 15; always KEYBOARD_IRQ
 *                for this function
 *         receives input from the keyboard I/O ports
 * OUTPUTS: none
 * RETURNS: void
 * SIDE EFFECTS: Can modify the screen content.
 */

static int keyboard_handler(uint32_t irq){
    int was_special = 0;
    int kill_proc = 0;
    cli();
    send_eoi(KEYBOARD_IRQ);
    // Read the scan code from the keyboard data port
    uint32_t scancode = inb(KEYBOARD_DATA_PORT); 
    int active_terminal_id = get_active_terminal_id();
    int terminal_to_switch = 0;

    
        switch (scancode) {
            case CTRL_PRESS_SCANCODE:
                ctrl_pressed = TRUE;        // Flag that the control key is pressed
                break;
            case CTRL_RELEASE_SCANCODE:
                ctrl_pressed = FALSE;       // Flag that the control key is released
                break;
            case LEFT_SHIFT_PRESS_SCANCODE:
            case RIGHT_SHIFT_PRESS_SCANCODE:
                shift_pressed = TRUE;       // Flag that either shift key is pressed
                break;
            case LEFT_SHIFT_RELEASE_SCANCODE:
            case RIGHT_SHIFT_RELEASE_SCANCODE:
                shift_pressed = FALSE;      // Flag that the shift key is released
                break;
            case CAPS_LOCK_SCANCODE:
                caps_lock_active = !caps_lock_active;       // Toggle caps lock state
                break;
            case LEFT_ALT_PRESS_SCANCODE:
            case RIGHT_ALT_PRESS_SCANCODE:
                alt_pressed = TRUE;
                break;
            case LEFT_ALT_RELEASE_SCANCODE:
            case RIGHT_ALT_RELEASE_SCANCODE:
                alt_pressed = FALSE;
                break;
            case TAB_SCANCODE:
            {
                // Handle the tab key; insert tab character to the buffer and screen
                int i;
                for (i = 0; i < 4; i++) {  // Assuming a tab size of 4 spaces
                    term_recv_byte(' ', active_terminal_id);
                }
                break;
            }
            case BACKSPACE_SCANCODE:
                // Handle backspace; remove characters from the buffer and screen
                if (terminals[active_terminal_id].buffer_index > 0) {
                    term_bksp(active_terminal_id);
                }
                break;
            default:
                // Handle special combinations like Ctrl+L
                if (ctrl_pressed == 1 && scancode == 0x26) {    // 0x26 is 'l'
                    term_clear();     // Clear the screen when Ctrl+L is pressed
                    was_special = 1;
                }
                if(ctrl_pressed == 1 && scancode == 0x2E) { // 0x2E is 'c'
                    was_special = kill_proc = 1;
                }
                if(ctrl_pressed && scancode == 0x25) { // 0x25 is 'k'
                    was_special = 1;
                    osk_enable = !osk_enable;
                }
                if(ctrl_pressed && scancode == 0x32) { // 0x32 is 'm'
                    was_special = 1;
                    cursor_enable = !cursor_enable;
                }
                if (alt_pressed && (scancode == F1_SCANCODE)) {
                    terminal_to_switch = 0;
                    switch_terminal(terminal_to_switch);
                    was_special = 1;
                }
                if (alt_pressed && (scancode == F2_SCANCODE)) {
                    terminal_to_switch = 1;
                    switch_terminal(terminal_to_switch);
                    was_special = 1;
                }
                if (alt_pressed && (scancode == F3_SCANCODE)) {
                    terminal_to_switch = 2;
                    switch_terminal(terminal_to_switch);
                    was_special = 1;
                }
                if(alt_pressed && ctrl_pressed && shift_pressed && scancode == 0x2D) { // 0x2D is 'x'
                    display_xenia();
                    was_special = 1;
                }
                // if(!was_special && terminals[active_terminal_id].term_in_flag == 1) {
                //     clear_keyboard_buffer();
                //     terminals[active_terminal_id].term_in_flag = 0;
                // }
                // Handle regular characters from index 0-127, and handle newline at index 127 (0x1C is Enter)
                // Convert scancode to ASCII if in valid range and add it to buffer if there's space
                if (scancode < NUM_SCANCODES && !was_special) {        // !ctrl_pressed is a safety check, don't want that
                    char ascii_char;
                    if (shift_pressed && !caps_lock_active) {
                        ascii_char = scancode_to_shifted_ascii[scancode];
                    } else if (caps_lock_active && !shift_pressed) {
                        ascii_char = scancode_to_capslock_ascii[scancode];
                    } else if (caps_lock_active && shift_pressed) {
                        ascii_char = scancode_to_capsshift_ascii[scancode];
                    } else {
                        ascii_char = scancode_to_ascii[scancode];
                    }
                    term_recv_byte(ascii_char, active_terminal_id);
                }
            }

    // we have to send eoi first before we do any process switching
    if(kill_proc) {
        pcb_t *pcb = get_current_pcb();
        /* note: since interrupt handlers can fire before the first process is started,
         * we have to check if the current pcb is actually present. checking this is fine,
         * since the process manager and pcb's are initialized to not present before
         * interrupts are enabled */
        if(pcb->present) kill_term_process(TERMINATED_STATUS);
    }
    return 1; // successfully serviced interrupt

}

