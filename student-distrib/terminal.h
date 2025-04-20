/* terminal.h -- contains definitions for the terminal abstraction */

#ifndef _TERMINAL_H
#define _TERMINAL_H

#define VIDEO_MEM_SIZE  4096
#define TERMINAL_VIDEO_MEM_SIZE (NUM_TERMINALS * VIDEO_MEM_SIZE)
#define KEYBOARD_BUFFER_SIZE    128
#define NUM_TERMINALS   3

#include "fd.h"

#ifndef ASM

/* receive an ascii character from the keyboard to the active terminal */
void term_recv_byte(char c, int terminal_id);

/* clear the active terminal screen */
void term_clear(void);

void init_terminals(void);
void start_terminals(void);
void putc(uint8_t c);
void term_putc(uint8_t c, int active_terminal_id);
void term_update_cursor(int row, int col, int terminal_id);
void clear_screen(void);
void term_clear_screen(int terminal_id);
void bksp(void);
void term_bksp(int terminal_id);
int get_active_terminal_id(void);
void switch_terminal(int new_terminal_id);
uint8_t *get_vidmem_loc(int terminal_id);
void term_clear_keyboard_buffer(int terminal_id);


typedef struct {
    int screen_x, screen_y;     // Cursor positions for each terminal
    char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
    int buffer_index;
    volatile int term_in_flag;
} terminal_t;
extern terminal_t terminals[NUM_TERMINALS];    // We have 3 terminals



extern fd_open_t term_open;
extern fd_close_t term_close;
extern fd_read_t term_read;
extern fd_read_t term_noread;
extern fd_write_t term_write;
extern fd_write_t term_nowrite;

extern fd_driver_t term_stdin_fd_driver;
extern fd_driver_t term_stdout_fd_driver;

#endif /* _TERMINAL_H */
#endif /* ASM */
