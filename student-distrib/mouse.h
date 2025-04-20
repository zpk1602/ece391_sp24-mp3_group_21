/* mouse.h - mouse driver definitions */

#ifndef _MOUSE_H
#define _MOUSE_H

#define MOUSE_LEFT 0x1
#define MOUSE_RIGHT 0x2
#define MOUSE_MIDDLE 0x4

#ifndef ASM

/* x and y absolute coordinates of the mouse, not bounded by any screen.
 * for mouse_x, left is negative and right is positive
 * for mouse_y, down is negative and up is positive */
extern int32_t mouse_x;
extern int32_t mouse_y;
/* the first byte of each packet from the mouse. in particular, anding with the macros
 * above will tell you which buttons are currently pressed. */
extern uint8_t mouse_buttons;

/* initialize the mouse driver */
void mouse_init(void);

#endif /* ASM */
#endif /* _MOUSE_H */
