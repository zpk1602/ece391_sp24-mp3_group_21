/* terminal.c -- terminal abstraction implementation */

#include "terminal.h"
#include "keyboard.h"
#include "lib.h"
#include "process.h"
#include "mm.h"

#define VIDEO_MEM_SIZE 4096     // 4KB block
#define NUM_TERMINALS 3
#define NUM_COLS    80
#define NUM_ROWS    25
#define ATTRIB      0x7
#define VGA_MEM_BASE    0xB8000     // The standard VGA text mode address

static int active_terminal_id = 0;  // Default the first terminal as active

/*
* FUNCTION: get_active_terminal_id
* DESCRIPTION: Returns the ID of the currently active terminal.
* INPUTS: none
* OUTPUTS: none
* RETURNS: The integer ID of the active terminal.
*/
int get_active_terminal_id(void) {
    return active_terminal_id;
}

typedef uint8_t video_mem_block[VIDEO_MEM_SIZE] __attribute__((aligned(VIDEO_MEM_SIZE)));

// 3 consecutive 4KB blocks after the main video memory and double buffer memory
video_mem_block *video_buffers = 2 + (video_mem_block*) VGA_MEM_BASE;

terminal_t terminals[NUM_TERMINALS]; // This actually creates the array.

/*
* FUNCTION: get_vidmem_loc
* DESCRIPTION: Gets the location of the video memory for a specific terminal ID.
* INPUTS: terminal_id - the ID of the terminal
* OUTPUTS: Pointer to the start of video memory for the given terminal.
* RETURNS: A pointer to the video memory location for the specified terminal.
*/
uint8_t *get_vidmem_loc(int terminal_id) {
    if (terminal_id < 0 || terminal_id >= NUM_TERMINALS) panic_msg("out of bounds TID");
    // return terminal_id == active_terminal_id ? (uint8_t*) VGA_MEM_BASE :
    //         video_buffers[terminal_id];
    return video_buffers[terminal_id];
}

/*
* FUNCTION: init_terminals
* DESCRIPTION: Initializes terminal video buffers to empty spaces and sets cursor position to the top-left corner.
* INPUTS: none
* OUTPUTS: Clears and resets video buffers for all terminals.
* RETURNS: void
*/
void init_terminals(void) {
    int i;
    for (i = 0; i < NUM_TERMINALS; i++) {
        // Clear the buffer
        memset_word(video_buffers[i], ((uint32_t) ' ') | 0x700, NUM_ROWS * NUM_COLS);
    }
}

/* start_terminals
 * Allocates a shell per terminal, and then jumps to the first terminal's shell.
 * Assumes that there is at least 1 terminal.
 * Return value: never returns
 * Side effects: Allocates processes and jumps to them. 
 */
void start_terminals(void) {
    if(NUM_TERMINALS <= 0) panic_msg("non-positive number of terminals?");
    cli();
    pcb_t *first = alloc_process(NULL, (uint8_t*) "shell", 0);
    int i;
    for(i = 1; i < NUM_TERMINALS; ++i) {
        alloc_process(NULL, (uint8_t*) "shell", i);
    }
    jump_to_process(first);
}

/*
* FUNCTION: switch_terminal
* DESCRIPTION: Switches from the current active terminal to a new terminal. 
*              It saves the state of the current terminal and loads the state of the new one.
* INPUTS: new_terminal_id - the ID of the terminal to switch to.
* OUTPUTS: The active terminal display is switched, and the process state is potentially updated.
* RETURNS: void
*/
void switch_terminal(int new_terminal_id) {
    // Validate terminal ID and check if it's already active
    if (new_terminal_id == active_terminal_id || new_terminal_id < 0 || new_terminal_id >= NUM_TERMINALS)
        return;

    // Disable interrupts to prevent race conditions during the switch
    uint32_t flags;
    cli_and_save(flags);

    // Determine if a TLB flush is needed by comparing current and new terminal IDs
    // pcb_t *curr_pcb = get_current_pcb();
    // int need_tlb_flush = curr_pcb->terminal_id == active_terminal_id ||
    //         curr_pcb->terminal_id == new_terminal_id;

    // Save the video buffer of the currently active terminal
    // memcpy(video_buffers[active_terminal_id], (void*)VGA_MEM_BASE, VIDEO_MEM_SIZE);

    // Update the active terminal ID
    active_terminal_id = new_terminal_id;

    // Load the video buffer of the new active terminal
    // memcpy((void*)VGA_MEM_BASE, video_buffers[active_terminal_id], VIDEO_MEM_SIZE);

    // Update cursor position to the new active terminal
    update_cursor(terminals[active_terminal_id].screen_y, terminals[active_terminal_id].screen_x);

    // if(need_tlb_flush) set_user_page(pcb_to_pid(curr_pcb));

    // Restore interrupts to their previous state
    restore_flags(flags);
}


/*
* FUNCTION: term_recv_byte
* DESCRIPTION: Receives a character from the keyboard and adds it to the terminal's input buffer.
* INPUTS: c - the ASCII value of the character received from the keyboard
          terminal_id - the terminal where the character should be received
* OUTPUTS: The character is added to the terminal's keyboard buffer and is also output to the terminal screen.
* RETURNS: void
*/
void term_recv_byte(char c, int terminal_id) { // gets called from keyboard.c puts it in the line buffer.
    if (terminal_id < 0 || terminal_id >= NUM_TERMINALS) panic_msg("out of bounds TID");
    terminal_t *term = &terminals[terminal_id];
    if(term->term_in_flag == 1) {
        /* if there was a complete buffer already in the buffer, clear it to make way
         * for the new byte */
        term_clear_keyboard_buffer(terminal_id);
        term->term_in_flag = 0;
    }
    if (c != '\0' && (term->buffer_index < KEYBOARD_BUFFER_SIZE - 1 ||
            (term->buffer_index == KEYBOARD_BUFFER_SIZE - 1 && c == '\n'))) {
        term->keyboard_buffer[term->buffer_index++] = c;
        // Echo character
        term_putc(c, terminal_id);
        if (c == '\n') {
            // Handle newline
           term->term_in_flag = 1;
        }
    }
}

/*
* FUNCTION: term_clear
* DESCRIPTION: Clears the display screen.
* INPUTS: none
* OUTPUTS: Cleared display screen.
* RETURNS: void
*/
void term_clear(void) {
    clear_screen();
}

/* term_open(fd_info_t *fd_info, const uint8_t *filename);
 * DESCRIPTION: Opens the RTC file'
 * INPUTS: fd_info - file descriptor info struct'
 *         filename - name of the file to open
 * OUTPUTS: none
 * RETURNS: 0 on success, -1 on failure
 */
int32_t term_open(fd_info_t *fd_info, const uint8_t *filename) {
    //TODO: initilaze terminal stuff or nothing
    // fd_info->file_ops = &term_fd_driver;
    fd_info->inode = 0;
    fd_info->file_pos = 0;
    return 0;
}

/*
* FUNCTION: term_close
* DESCRIPTION: Closes the RTC file'
* INPUTS: fd_info - file descriptor info struct'
* OUTPUTS: none
* RETURNS: 0 on success, -1 on failure
*/
int32_t term_close(fd_info_t *fd_info) {
    //TODO: clears any terminal specific variables or nothing
    return 0;
}

/*
* term_read
* DESCRIPTION: Reads from the RTC file'
* INPUTS: fd_info - file descriptor info struct'
*         buf - buffer to read into
*         nbytes - number of bytes to read
* OUTPUTS: none
* RETURNS: 0 on success, -1 on failure
*/
int32_t term_read(fd_info_t *fd_info, void *buf, int32_t nbytes) { 
    //waits for a line of input, then when enter is pressed then it returns the to the calling function. (similar to rtc read)
    //TODO: reads from keyboard buffer into buf, return number of bytes read.
    //TODO: buffer overflow check
    // should be able to handle buffer overflow (User types more than 127 characters) and situations
    // where the size sent by the user is smaller than 128 characters.
    // If the user fills up one line by typing and hasn’t typed 128 characters yet,
    // you should roll over to the next line.
    // Backspace on the new line should go back to the previous line as well
    uint32_t flags;
    if (buf == NULL || nbytes < 0) return -1;
    pcb_t *curr_pcb = get_current_pcb();
    terminal_t *term = &terminals[curr_pcb->terminal_id];
    while(!term->term_in_flag) { // wait for enter to be pressed
        asm volatile("hlt");
    }
    cli_and_save(flags);
    //
    int i; // the number of bytes copied.
    for(i = 0; i < nbytes; i++) { // for nbytes wanted to be read
        if(i >= term->buffer_index) break;  // if buffer overflow, break
        ((char*)buf)[i] = term->keyboard_buffer[i];
    }

    term_clear_keyboard_buffer(curr_pcb->terminal_id);
    term->term_in_flag = 0;
    restore_flags(flags);
    return i;
}

/*
* term_write
* DESCRIPTION: Writes to the RTC file'
* INPUTS: fd_info - file descriptor info struct'
*         buf - buffer to write from
*         nbytes - number of bytes to write
* OUTPUTS: none
* RETURNS: 0 on success, -1 on failure
*/
int32_t term_write(fd_info_t *fd_info, const void *buf, int32_t nbytes) {
    //TODO: writes to the screen from buf, return number of bytes written or -1 on failure.
    if(!buf || nbytes < 0) return -1;
    pcb_t *curr_pcb = get_current_pcb();
    int i;
    for(i = 0; i < nbytes; i++) {
        term_putc(((char*)buf)[i], curr_pcb->terminal_id);
    }
    return nbytes;
}

/*
* term_noread
* DESCRIPTION: Always fails, used for stdout'
* INPUTS: fd_info - file descriptor info struct'
*         buf - buffer to read into
*         nbytes - number of bytes to read
* OUTPUTS: none
* RETURNS: 0 on success, -1 on failure
*/
int32_t term_noread(fd_info_t *fd_info, void *buf, int32_t nbytes) {
    return -1;
}

/*
* term_nowrite
* DESCRIPTION: Always fails, used for stdin'
* INPUTS: fd_info - file descriptor info struct'
*         buf - buffer to read into
*         nbytes - number of bytes to read
* OUTPUTS: none
* RETURNS: 0 on success, -1 on failure
*/
int32_t term_nowrite(fd_info_t *fd_info, const void *buf, int32_t nbytes) {
    return -1;
}

/*
* term_stdin_fd_driver
* DESCRIPTION: Jump table for the standard input fd driver
*/
fd_driver_t term_stdin_fd_driver = {
    .open = term_open,
    .close = term_close,
    .read = term_read,
    .write = term_nowrite,
};

/*
* term_stdout_fd_driver
* DESCRIPTION: Jump table for the standard input fd driver
*/
fd_driver_t term_stdout_fd_driver = {
    .open = term_open,
    .close = term_close,
    .read = term_noread,
    .write = term_write,
};


/*
 * FUNCTION: clear_keyboard_buffer
 * DESCRIPTION: Clears the keyboard buffer by setting all elements to 0.
 * INPUTS: none
 * OUTPUTS: Clears the active terminal's keyboard buffer and resets the index.
 * RETURNS: void
 * SIDE EFFECTS: Allows for new inputs to be captured.
 */
void clear_keyboard_buffer(void) {
    int active_terminal_id = get_active_terminal_id();
    memset(terminals[active_terminal_id].keyboard_buffer, 0, KEYBOARD_BUFFER_SIZE);
    terminals[active_terminal_id].buffer_index = 0;
}


/*
 * FUNCTION: term_clear_keyboard_buffer
 * DESCRIPTION: Clears the keyboard buffer for a given terminal by setting all elements to 0.
 * INPUTS: terminal_id -- ID of the terminal whose keyboard buffer is to be cleared.
 * OUTPUTS: none
 * RETURNS: void
 * SIDE EFFECTS: Allows for new inputs to be captured.
 */
void term_clear_keyboard_buffer(int terminal_id) {
    if (terminal_id < 0 || terminal_id >= NUM_TERMINALS) panic_msg("out of bounds TID");
    terminal_t *term = &terminals[terminal_id];
    memset(term->keyboard_buffer, 0, KEYBOARD_BUFFER_SIZE);
    term->buffer_index = 0;
}


/*
* FUNCTION: putc
* DESCRIPTION: Outputs a character to the active terminal's display.
* INPUTS: c - character to be printed.
* OUTPUTS: Character printed to the display of the active terminal.
* RETURNS: void
*/
void putc(uint8_t c) {
    term_putc(c, active_terminal_id);
}


/*
* FUNCTION: term_putc
* DESCRIPTION: Outputs a character to the display of a specific terminal.
* INPUTS: c - character to be printed.
          terminal_id - terminal where the character will be printed.
* OUTPUTS: Character printed to the given terminal's display.
* RETURNS: void
*/
void term_putc(uint8_t c, int terminal_id) {
    if (terminal_id < 0 || terminal_id >= NUM_TERMINALS) panic_msg("out of bounds TID");
    // Access the correct terminal
    terminal_t *term = &terminals[terminal_id];
    
    // Choose the correct video memory buffer depending on whether the terminal is active
    uint8_t *video_buf = (uint8_t *)(video_buffers[terminal_id]);
    // if (terminal_id == get_active_terminal_id()) {
    //     video_buf = (uint8_t *)(VGA_MEM_BASE); // The terminal is active, use the actual video memory
    // } else {
    //     video_buf = (uint8_t *)(video_buffers[terminal_id]); // The terminal is not active, use its buffer
    // }
    
    // Disable interrupts
    uint32_t flags;
    cli_and_save(flags);

    if (c == '\t') {    // tab
        int i;
        for (i = 0; i < 4; i++) {
            term_putc(' ', terminal_id);
        }
    } else if (c == '\n' || c == '\r') {    // new line or return
        term->screen_y++;
        term->screen_x = 0;
    } else {
        *(video_buf + ((NUM_COLS * term->screen_y + term->screen_x) << 1)) = c;
        *(video_buf + ((NUM_COLS * term->screen_y + term->screen_x) << 1) + 1) = ATTRIB;
        term->screen_x++;
        term->screen_y += term->screen_x / NUM_COLS;
        term->screen_x %= NUM_COLS;
    }

    if (term->screen_y >= NUM_ROWS) {
        // Scroll up the terminal
        memmove(video_buf, video_buf + (NUM_COLS << 1), (NUM_ROWS - 1) * NUM_COLS * 2);
        // Clear the last line
        memset_word(video_buf + ((NUM_ROWS - 1) * NUM_COLS << 1), ' ' | (ATTRIB << 8), NUM_COLS);
        term->screen_y = NUM_ROWS - 1;
    }

    term_update_cursor(term->screen_y, term->screen_x, terminal_id);
    restore_flags(flags);
}


/* void update_cursor(int row, int col);
 * Inputs: int row -- the row where the cursor should be set
 *         int col -- the column where the cursor should be set
 * Return Value: none
 * FUNCTION: Moves the blinking cursor to the specified location on the screen. */
void update_cursor(int row, int col) {

    term_update_cursor(row, col, active_terminal_id);
}


/* void update_cursor(int row, int col, int terminal_id);
 * Inputs: int row -- the row where the cursor should be set
 *         int col -- the column where the cursor should be set
 *         int terminal_id -- the termianl where the cursor should be set
 * Return Value: none
 * FUNCTION: Moves the blinking cursor to the specified location on the screen. */
void term_update_cursor(int row, int col, int terminal_id) {
    if (terminal_id < 0 || terminal_id >= NUM_TERMINALS) panic_msg("out of bounds TID");
    if (terminal_id != get_active_terminal_id()) {
        return; // Only update the cursor for the active terminal
    }


    uint16_t pos = (row * NUM_COLS) + col;

    // disable interrupts so that our IO doesn't get interrupted
    uint32_t flags;
    cli_and_save(flags);
    outb(0x0E, 0x3D4);
    outb((uint8_t)(pos >> 8), 0x3D5);
    outb(0x0F, 0x3D4);
    outb((uint8_t)(pos & 0xFF), 0x3D5);
    restore_flags(flags);
}


/*
* FUNCTION: clear_screen
* DESCRIPTION: Function to clear the screen of the active terminal.
* INPUTS: none
* OUTPUTS: Calls term_clear_screen to clear the active terminal.
* RETURNS: void
*/
void clear_screen(void) {
    term_clear_screen(active_terminal_id);
}


/*
* FUNCTION: term_clear_screen
* DESCRIPTION: Clears the display screen and resets cursor position for a specified terminal.
* INPUTS: terminal_id - the ID of the terminal to clear and reset.
* OUTPUTS: Cleared video buffer and reset cursor position for the given terminal.
* RETURNS: void
*/
void term_clear_screen(int terminal_id) {
    // Make sure terminal_id is valid
    if (terminal_id < 0 || terminal_id >= NUM_TERMINALS) return;

    uint32_t flags;
    cli_and_save(flags);
    
    // Clear the specific terminal's video buffer
    memset_word(video_buffers[terminal_id], ((uint32_t) ' ') | 0x700, NUM_ROWS * NUM_COLS);
    
    // Reset cursor position for the specific terminal
    terminals[terminal_id].screen_x = 0;
    terminals[terminal_id].screen_y = 0;
    
    // If the specified terminal is active, update the actual display and cursor
    if (terminal_id == active_terminal_id) {
        // memcpy((void*)VGA_MEM_BASE, video_buffers[terminal_id], VIDEO_MEM_SIZE);
        term_update_cursor(0, 0, terminal_id);
    }

    restore_flags(flags);
}


/*
* FUNCTION: bksp
* DESCRIPTION: Deletes the last character entered in the active terminal.
* INPUTS: none
* OUTPUTS: Updates display and buffer of the active terminal.
* RETURNS: void
* SIDE EFFECTS: Moves the cursor back one position, deleting the character there.
*/
void bksp(void) {
    term_bksp(active_terminal_id);
}


/*
* FUNCTION: term_bksp
* DESCRIPTION: Deletes the last character entered for a specified terminal.
* INPUTS: terminal_id - ID of the terminal.
* OUTPUTS: Updates display and buffer for the given terminal.
* RETURNS: void
* SIDE EFFECTS: Moves the cursor back one position on the specified terminal, deleting the character there.
*/
void term_bksp(int terminal_id) {
    if (terminal_id < 0 || terminal_id >= NUM_TERMINALS) panic_msg("out of bounds TID");
    uint32_t flags;
    cli_and_save(flags);

    terminal_t *term = &terminals[terminal_id];
    // uint8_t *video_buf = (terminal_id == get_active_terminal_id()) ? (uint8_t *)(VGA_MEM_BASE) : (uint8_t *)(video_buffers[terminal_id]);
    uint8_t *video_buf = (uint8_t *)(video_buffers[terminal_id]);

    // Check if we're not at the very beginning
    if(term->buffer_index > 0) {
        term->buffer_index--;  // Decrement the buffer index
        int iters = 1;
        if(term->keyboard_buffer[term->buffer_index] == '\t') iters = 4;
        int i;
        for(i = 0; i < iters; ++i) {
            if(term->screen_x == 0 && term->screen_y > 0) {
                term->screen_y--;
                term->screen_x = NUM_COLS - 1; // Move to the end of the previous line
            } else {
                term->screen_x--;
            }

            // Clear the character at the new cursor position
            *(video_buf + ((NUM_COLS * term->screen_y + term->screen_x) << 1)) = ' ';
            *(video_buf + ((NUM_COLS * term->screen_y + term->screen_x) << 1) + 1) = ATTRIB;
        }
        
        // If this is the active terminal, update the cursor
        if (terminal_id == get_active_terminal_id()) {
            update_cursor(term->screen_y, term->screen_x);
        }
    }

    restore_flags(flags);
}

