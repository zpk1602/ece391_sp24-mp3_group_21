#ifndef _KEYBOARD_H
#define _KEYBOARD_H



#define  KEYBOARD_DATA_PORT   0x60
#define  KEYBOARD_STATUS_PORT   0x64
#define  KEYBOARD_IRQ          1

#define CTRL_PRESS_SCANCODE             0x1D
#define CTRL_RELEASE_SCANCODE           0x9D
#define LEFT_SHIFT_PRESS_SCANCODE       0x2A
#define RIGHT_SHIFT_PRESS_SCANCODE      0x36
#define LEFT_SHIFT_RELEASE_SCANCODE     0xAA
#define RIGHT_SHIFT_RELEASE_SCANCODE    0xB6
#define CAPS_LOCK_SCANCODE              0x3A
#define TAB_SCANCODE                    0x0F
#define BACKSPACE_SCANCODE              0x0E
#define F1_SCANCODE                     0x3B
#define F2_SCANCODE                     0x3C
#define F3_SCANCODE                     0x3D
#define LEFT_ALT_PRESS_SCANCODE         0x38
#define RIGHT_ALT_PRESS_SCANCODE        ((0xE0 << 8) | 0x38)
#define LEFT_ALT_RELEASE_SCANCODE       0xB8
#define RIGHT_ALT_RELEASE_SCANCODE      ((0xE0 << 8) | 0xB8)


#define FALSE   0
#define TRUE    1

#define BUFFER_SIZE     128

// Define the array size. We look at values
// up to but not including 0x58 which is 88 in decimal
#define  NUM_SCANCODES        88

// extern char keyboard_buffer[BUFFER_SIZE];
// extern int buffer_index;

// Initializes the keyboard
void keyboard_init();

// Clears the buffer of the keyboard, allowing new inputs
void clear_keyboard_buffer();


#endif 

