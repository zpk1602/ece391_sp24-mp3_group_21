/* gui.c - basic graphical user interface implementation
 * including the OSK (On Screen Keyboard) */

#include "types.h"
#include "lib.h"
#include "terminal.h"
#include "mouse.h"
#include "process.h"
#include "xenia_vga.h"
#include "gui.h"

/* how many "steps" reported by the mouse count as one screen character. */
#define MOUSE_SPEED 24

#define OSK_WIDTH 34 // width of keyboard on screen
#define OSK_HEIGHT 8 // height of keyboard on screen
#define ATTRIB_DEFAULT 0x7 // default color of all terminal text
#define ATTRIB_OSK 0x5 // color of the keyboard normally
#define ATTRIB_PRESS 0xD0 // color when clicking on a button
#define ATTRIB_ON 0x50 // color when a button was toggled on (eg caps lock)
#define ATTRIB_PTR 0xB // color of the mouse cursor

#define VGA_MEM_BASE 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// top left corner position on the screen of the keyboard. centered on bottom edge
#define OSK_X_POS ((VGA_WIDTH-OSK_WIDTH)/2)
#define OSK_Y_POS (VGA_HEIGHT - OSK_HEIGHT)
// number of toggleable buttons on the keyboard (both shifts, ctrls, alts, plus caps lock)
#define OSK_NUM_TOGGLE 7

/* C strings containing the visual keyboard sprite for various combinations
 * of caps lock and shift. since the rows have a constant
 * width, we don't need to store a newline delimiter at all. */
static const uint8_t *osk_string = (const uint8_t *)
" ________________________________ "
"|esc f1 f2 f3 f4 f5 f6 f7 f8 f9  |"
"| ` 1 2 3 4 5 6 7 8 9 0 - = bksp |"
"|tab q w e r t y u i o p [ ] \\   |"
"|caps a s d f g h j k l ; ' enter|"
"|shift z x c v b n m , . /  shift|"
"|ctrl alt spaaaaaace alt ctrl    |"
" -------------------------------- ";

static const uint8_t *osk_string_caps = (const uint8_t *)
" ________________________________ "
"|esc f1 f2 f3 f4 f5 f6 f7 f8 f9  |"
"| ` 1 2 3 4 5 6 7 8 9 0 - = bksp |"
"|tab Q W E R T Y U I O P [ ] \\   |"
"|caps A S D F G H K K L ; ' enter|"
"|shift Z X C V B N M , . /  shift|"
"|ctrl alt spaaaaaace alt ctrl    |"
" -------------------------------- ";

static const uint8_t *osk_string_shift = (const uint8_t *)
" ________________________________ "
"|esc f1 f2 f3 f4 f5 f6 f7 f8 f9  |"
"| ~ ! @ # $ % ^ & * ( ) _ + bksp |"
"|tab Q W E R T Y U I O P { } |   |"
"|caps A S D F G H J K L : \" enter|"
"|shift Z X C V B N M < > ?  shift|"
"|ctrl alt spaaaaaace alt ctrl    |"
" -------------------------------- ";

static const uint8_t *osk_string_caps_shift = (const uint8_t *)
" ________________________________ "
"|esc f1 f2 f3 f4 f5 f6 f7 f8 f9  |"
"| ~ ! @ # $ % ^ & * ( ) _ + bksp |"
"|tab q w e r t y u i o p { } |   |"
"|caps a s d f g h j k l : \" enter|"
"|shift z x c v b n m < > ?  shift|"
"|ctrl alt spaaaaaace alt ctrl    |"
" -------------------------------- ";

/* toggleables */
/* the OSK_* constants are keycodes, while the GUI_* constants are indices into
 * gui_toggleable */
#define OSK_TOGGLEABLE 0x100
#define GUI_LSHIFT 0
#define OSK_LSHIFT 0x100
#define GUI_RSHIFT 1
#define OSK_RSHIFT 0x101
#define GUI_LCTRL 2
#define OSK_LCTRL 0x102
#define GUI_RCTRL 3
#define OSK_RCTRL 0x103
#define GUI_LALT 4
#define OSK_LALT 0x104
#define GUI_RALT 5
#define OSK_RALT 0x105
#define GUI_CAPS 6
#define OSK_CAPS 0x106

/* not toggleables, but still key codes */
#define OSK_ESCAPE 0x200
#define OSK_BKSP 0x201
/* f1 has key code OSK_FN+1, f2 has OSK_FN+2, etc */
#define OSK_FN 0x300

/* the order of the keys along the on screen keyboard. used to populate
 * the osk_codes_* arrays dynamically at initialization. */
static uint16_t osk_keys[] = {
    OSK_ESCAPE, OSK_FN+1, OSK_FN+2, OSK_FN+3, OSK_FN+4, OSK_FN+5, OSK_FN+6, OSK_FN+7, OSK_FN+8, OSK_FN+9,
    '`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', OSK_BKSP,
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\',
    OSK_CAPS, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '\n',
    OSK_LSHIFT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', OSK_RSHIFT,
    OSK_LCTRL, OSK_LALT, ' ', OSK_RALT, OSK_RCTRL,
};
static uint16_t osk_keys_caps[] = {
    OSK_ESCAPE, OSK_FN+1, OSK_FN+2, OSK_FN+3, OSK_FN+4, OSK_FN+5, OSK_FN+6, OSK_FN+7, OSK_FN+8, OSK_FN+9,
    '`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', OSK_BKSP,
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\\',
    OSK_CAPS, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '\n',
    OSK_LSHIFT, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', OSK_RSHIFT,
    OSK_LCTRL, OSK_LALT, ' ', OSK_RALT, OSK_RCTRL,
};
static uint16_t osk_keys_shift[] = {
    OSK_ESCAPE, OSK_FN+1, OSK_FN+2, OSK_FN+3, OSK_FN+4, OSK_FN+5, OSK_FN+6, OSK_FN+7, OSK_FN+8, OSK_FN+9,
    '~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', OSK_BKSP,
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '|',
    OSK_CAPS, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '\n',
    OSK_LSHIFT, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', OSK_RSHIFT,
    OSK_LCTRL, OSK_LALT, ' ', OSK_RALT, OSK_RCTRL,
};
static uint16_t osk_keys_caps_shift[] = {
    OSK_ESCAPE, OSK_FN+1, OSK_FN+2, OSK_FN+3, OSK_FN+4, OSK_FN+5, OSK_FN+6, OSK_FN+7, OSK_FN+8, OSK_FN+9,
    '~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', OSK_BKSP,
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '|',
    OSK_CAPS, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '\n',
    OSK_LSHIFT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?', OSK_RSHIFT,
    OSK_LCTRL, OSK_LALT, ' ', OSK_RALT, OSK_RCTRL,
};

/* mappings from positions on the keyboard to the key code if there is a button there,
 * otherwise to 0 if there is no button. */
static uint16_t osk_codes[OSK_HEIGHT][OSK_WIDTH];
static uint16_t osk_codes_caps[OSK_HEIGHT][OSK_WIDTH];
static uint16_t osk_codes_shift[OSK_HEIGHT][OSK_WIDTH];
static uint16_t osk_codes_caps_shift[OSK_HEIGHT][OSK_WIDTH];

/* vga video memory storage for just the keyboard */
static uint8_t osk_vga[OSK_HEIGHT][OSK_WIDTH][2];
int osk_enable = 0; // is the on screen keyboard visible and interactable?
int cursor_enable = 0; // is the cursor visible and usable?

/* init_gui - Initialize the GUI and on screen keyboard. */
void init_gui(void) {
    int i,j;
    // copy osk_string to osk_vga and set initial attrib
    for(i = 0; i < OSK_HEIGHT; ++i) {
        for(j = 0; j < OSK_WIDTH; ++j) {
            osk_vga[i][j][0] = osk_string[j + i*OSK_WIDTH];
            osk_vga[i][j][1] = ATTRIB_OSK;
        }
    }
    // initialize lookup table mapping osk screen position to key code
    int pos = 0;
    for(i = 1; i < OSK_HEIGHT-1; ++i) {
        for(j = 1; j < OSK_WIDTH-1; ++j) {
            if(osk_string[j + i*OSK_WIDTH] == ' ') continue;
            while(j < OSK_WIDTH-1 && osk_string[j+i*OSK_WIDTH] != ' ') {
                osk_codes[i][j] = osk_keys[pos];
                osk_codes_caps[i][j] = osk_keys_caps[pos];
                osk_codes_shift[i][j] = osk_keys_shift[pos];
                osk_codes_caps_shift[i][j++] = osk_keys_caps_shift[pos];
            }
            pos++;
        }
    }
}

/* set_vga_start - Changes the VGA hardware to think the provided host CPU pointer
 * is the start of video memory. This pointer must be in the range of video memory,
 * from 0xB8000 to 0xB9999.
 * Inputs: vga - The first address of video memory. Must be aligned to at least
 *               a 4KB page boundary.
 * Side effects: writes to VGA registers. */
void set_vga_start(void *vga) {
    if((uint32_t)vga & ((1 << 12) - 1)) panic_msg("vga not 4KB aligned");
    // vga memory is usually a 32kB region starting at 0xB8000,
    // i.e. the Memory Map Select Field is set to 3 typically
    if((uint32_t)vga < 0xB8000 || (uint32_t)vga >= 0xC0000)
        panic_msg("vga out of bounds!");
    // write to Start Address High Register at index 0xC
    /* the right shift is due to this address in the VGA hardware addressing 16 bit words
     * on the host side, not bytes, so we need to divide by two to get the correct address. */
    outw(0xC | ((((uint32_t)vga - 0xB8000) >> 1) & 0xFF00), 0x3D4);
}

/* the current buffer we are rendering to. since we double buffer to prevent
 * flickering, we need to keep track of the last of the two buffer that was used. */
static uint16_t *gui_vga_ptr = (uint16_t*) VGA_MEM_BASE;

/* offset used to prevent the cursor getting stuck at the screen edges.
 * move past the screen edges adjusts this, so to the rest of the function the cursor
 * still appears to be right at the edge. */
static int cursor_offset_x = 0;
static int cursor_offset_y = 0;
static int was_pressed = 0; // whether the left mouse button was pressed last we checked.
// where the mouse press+drag started
static int press_start_row;
static int press_start_col;
// the last location we saw the cursor in
static int cursor_prev_row = 0;
static int cursor_prev_col = 0;
// array of booleans for whether each toggleable key is currently held down or not
static uint8_t gui_toggleable[OSK_NUM_TOGGLE];

/* osk_deselect - removes the highlight on a button now that we are no longer
 * hovering over it.
 * Inputs: cursor_row/col - the position of the button (if there was one) in screen coords */
static void osk_deselect(int cursor_row, int cursor_col) {
    if(cursor_row < OSK_Y_POS || cursor_row >= OSK_Y_POS+OSK_HEIGHT ||
            cursor_col < OSK_X_POS || cursor_col >= OSK_X_POS+OSK_WIDTH) return;
    cursor_row -= OSK_Y_POS; cursor_col -= OSK_X_POS;
    uint16_t key = osk_codes[cursor_row][cursor_col];
    if(!key) return;

    uint8_t attrib = ATTRIB_OSK;

    /* if the given button is toggleable and is currently held, we should revert
     * back to the on attribute, not the default attribute */
    if((key & 0xFF00) == OSK_TOGGLEABLE) {
        int toggle = key - OSK_TOGGLEABLE;
        if(toggle >= OSK_NUM_TOGGLE) panic_msg("out of bounds toggle!");
        if(gui_toggleable[toggle]) attrib = ATTRIB_ON;
    }

    // move to the left edge of button
    for(; osk_codes[cursor_row][cursor_col]; cursor_col--);
    cursor_col++; // move to first char in button
    for(; osk_codes[cursor_row][cursor_col]; cursor_col++) {
        osk_vga[cursor_row][cursor_col][1] = attrib;
    }
}
/* osk_deselect - adds the highligh to a button now that we are hovering over it.
 * Inputs: cursor_row/col - the position of the button (if there was one) in screen coords */
static void osk_select(int cursor_row, int cursor_col) {
    if(cursor_row < OSK_Y_POS || cursor_row >= OSK_Y_POS+OSK_HEIGHT ||
            cursor_col < OSK_X_POS || cursor_col >= OSK_X_POS+OSK_WIDTH) return;
    cursor_row -= OSK_Y_POS; cursor_col -= OSK_X_POS;
    if(!osk_codes[cursor_row][cursor_col]) return;

    // move to the left edge of button
    for(; osk_codes[cursor_row][cursor_col]; cursor_col--);
    cursor_col++; // move to first char in button
    for(; osk_codes[cursor_row][cursor_col]; cursor_col++) {
        osk_vga[cursor_row][cursor_col][1] = ATTRIB_PRESS;
    }
}
/* osk_keypress - called upon releasing the left mouse button, indicating we should
 * actually perform the button press.
 * Inputs: cursor_row/col - the position of the button (if there was one) in screen coords
 * Side effects: Any special effects pressing a key on the keyboard can have, such
 *               as clearing the screen, killing a process, switching terminals, etc */
static void osk_keypress(int cursor_row, int cursor_col) {
    if(cursor_row < OSK_Y_POS || cursor_row >= OSK_Y_POS+OSK_HEIGHT ||
            cursor_col < OSK_X_POS || cursor_col >= OSK_X_POS+OSK_WIDTH) return;
    cursor_row -= OSK_Y_POS; cursor_col -= OSK_X_POS;
    if(!osk_codes[cursor_row][cursor_col]) return;

    uint16_t key;
    if(gui_toggleable[GUI_LSHIFT] || gui_toggleable[GUI_RSHIFT]) {
        if(gui_toggleable[GUI_CAPS]) key = osk_codes_caps_shift[cursor_row][cursor_col];
        else key = osk_codes_shift[cursor_row][cursor_col];
    } else {
        if(gui_toggleable[GUI_CAPS]) key = osk_codes_caps[cursor_row][cursor_col];
        else key = osk_codes[cursor_row][cursor_col];
    }
    // log_msg("keypress! code: 0x%x key: %c", key, key < 0x100 ? key : 0);
    if(key < 0x100) {
        int was_special = 0;
        int ctrl_pressed = gui_toggleable[GUI_LCTRL] || gui_toggleable[GUI_RCTRL];
        int alt_pressed = gui_toggleable[GUI_LALT] || gui_toggleable[GUI_RALT];
        int shift_pressed = gui_toggleable[GUI_LSHIFT] || gui_toggleable[GUI_RSHIFT];
        if(ctrl_pressed) {
            switch(osk_codes[cursor_row][cursor_col]) {
            case 'l':
                term_clear();
                was_special = 1;
                break;
            case 'k':
                osk_enable = !osk_enable;
                was_special = 1;
                break;
            case 'm':
                cursor_enable = !cursor_enable;
                was_special = 1;
                break;
            case 'c':
                {
                    pcb_t *curr_pcb = get_current_pcb();
                    if(curr_pcb->present) kill_term_process(TERMINATED_STATUS);
                    was_special = 1;
                }
                break;
            case 'x':
                if(alt_pressed && shift_pressed) {
                    display_xenia();
                    was_special = 1;
                }
                break;
            }
        }
        if(!was_special) term_recv_byte(key, get_active_terminal_id());
    } else if((gui_toggleable[GUI_LALT] || gui_toggleable[GUI_RALT]) &&
            (key & 0xFF00) == OSK_FN && (key - OSK_FN -1) < NUM_TERMINALS) {
        switch_terminal(key-OSK_FN-1);
    } else if(key == OSK_BKSP) bksp();
    else if((key & 0xFF00) == OSK_TOGGLEABLE) {
        int toggle = key - OSK_TOGGLEABLE;
        if(toggle >= OSK_NUM_TOGGLE) panic_msg("out of bounds toggle!");
        gui_toggleable[toggle] = !gui_toggleable[toggle];

        // move to the left edge of button
        for(; osk_codes[cursor_row][cursor_col]; cursor_col--);
        cursor_col++; // move to first char in button
        for(; osk_codes[cursor_row][cursor_col]; cursor_col++) {
            osk_vga[cursor_row][cursor_col][1] = gui_toggleable[toggle] ?
                    ATTRIB_ON : ATTRIB_OSK;
        }

        const uint8_t *new_string;
        int i, j;
        switch(key) {
        case OSK_LSHIFT:
        case OSK_RSHIFT:
        case OSK_CAPS:
            if(gui_toggleable[GUI_LSHIFT] || gui_toggleable[GUI_RSHIFT]) {
                new_string = gui_toggleable[GUI_CAPS] ? osk_string_caps_shift :
                        osk_string_shift;
            } else {
                new_string = gui_toggleable[GUI_CAPS] ? osk_string_caps :
                        osk_string;
            }
            // copy new osk_string to osk_vga and set initial attrib
            for(i = 0; i < OSK_HEIGHT; ++i) {
                for(j = 0; j < OSK_WIDTH; ++j) {
                    osk_vga[i][j][0] = new_string[j + i*OSK_WIDTH];
                }
            }
            break;
        }
    }
}
/* intermediate function for when the mouse button is first pressed
 * Inputs: cursor_row/col - the position of the button (if there was one) in screen coords */
static void gui_handle_mouse_press(int cursor_row, int cursor_col) {
    osk_select(cursor_row, cursor_col);
}
/* intermediate function for when we drag the mouse while the LMB (left mouse button) is held
 * Inputs: cursor_row/col - the position of the button (if there was one) in screen coords */
static void gui_handle_mouse_drag(int cursor_row, int cursor_col) {
    osk_deselect(cursor_prev_row, cursor_prev_col);
    osk_select(cursor_row, cursor_col);
}
/* intermediate function for when the LMB is released, indicating the end of a drag and
 * the firing of the actual keypress.
 * Inputs: cursor_row/col - the position of the button (if there was one) in screen coords
 * Side effects: Any keybind effects, killing process, switching terminals, etc. */
static void gui_handle_mouse_release(int cursor_row, int cursor_col) {
    osk_deselect(cursor_prev_row, cursor_prev_col);
    osk_keypress(cursor_row, cursor_col);
}

/* do_render - Renders the backing terminal buffers to the screen, layering
 * the on screen keyboard and the mouse cursor on top of it. This is called by the
 * PIT handler to regularly redraw the screen. It also checks for mouse inputs
 * and acts accordingly. 
 * Side effects: Keypress keybind actions: killing processes, switching terminals, etc */
void do_render(void) {

    uint16_t *vidmap = (uint16_t*) get_vidmem_loc(get_active_terminal_id());

    int new_pressed = !!(mouse_buttons & MOUSE_LEFT);
    int cursor_row, cursor_col;
    if(cursor_enable) {
        /* vga coords go negative to positive top to bottom, while cursor coords
         * go bottom to top, so a negative sign is needed */
        cursor_row = floor_div(cursor_offset_y - mouse_y, MOUSE_SPEED);
        if(cursor_row < 0) {
            cursor_offset_y += -cursor_row * MOUSE_SPEED;
            cursor_row = 0;
        } else if(cursor_row > VGA_HEIGHT-2) {
            cursor_offset_y -= (cursor_row - (VGA_HEIGHT-2)) * MOUSE_SPEED;
            cursor_row = VGA_HEIGHT-2;
        }
        cursor_col = floor_div(cursor_offset_x + mouse_x, MOUSE_SPEED);
        if(cursor_col < 0) {
            cursor_offset_x += -cursor_col * MOUSE_SPEED;
            cursor_col = 0;
        } else if(cursor_col > VGA_WIDTH-1) {
            cursor_offset_x -= (cursor_col - (VGA_WIDTH-1)) * MOUSE_SPEED;
            cursor_col = VGA_WIDTH-1;
        }
        if(mouse_buttons & MOUSE_RIGHT) {
            uint16_t *cursor = &vidmap[cursor_col + cursor_row*VGA_WIDTH];
            *cursor = *cursor + 0x1100;
        }
        if(osk_enable) {
            if(new_pressed - was_pressed == 1) {
                press_start_row = cursor_row;
                press_start_col = cursor_col;
                cursor_prev_row = cursor_row;
                cursor_prev_col = cursor_col;
                gui_handle_mouse_press(cursor_row, cursor_col);
            } else if(new_pressed - was_pressed == -1) {
                gui_handle_mouse_release(cursor_row, cursor_col);
            } else if(new_pressed && (cursor_row != cursor_prev_row ||
                    cursor_col != cursor_prev_col)) {
                gui_handle_mouse_drag(cursor_row, cursor_col);
                cursor_prev_row = cursor_row;
                cursor_prev_col = cursor_col;
            }
        }
    }

    // toggle which of two 4KB pages we use for double buffering
    gui_vga_ptr = (uint16_t*) ((uint32_t)gui_vga_ptr ^ (1 << 12));
    memcpy(gui_vga_ptr, vidmap, 2*VGA_WIDTH*VGA_HEIGHT);

    if(osk_enable) {
        uint16_t *vga = gui_vga_ptr + OSK_X_POS + OSK_Y_POS*VGA_WIDTH;
        int i;
        for(i = 0; i < OSK_HEIGHT; ++i) {
            memcpy(vga, &osk_vga[i], 2*OSK_WIDTH);
            vga += VGA_WIDTH;
        }
    }
    if(cursor_enable) {
        gui_vga_ptr[cursor_col + (cursor_row+1)*VGA_WIDTH] = '^' | (ATTRIB_PTR << 8);
    }
    set_vga_start(gui_vga_ptr);
    was_pressed = new_pressed;
}

/* you like breaking userspace, don't you?
 * draws the best linux mascot to the screen; a easter egg of sorts. */
void display_xenia(void) {
    memcpy(get_vidmem_loc(get_active_terminal_id()), xenia_vga, VGA_WIDTH*VGA_HEIGHT*2);
}
