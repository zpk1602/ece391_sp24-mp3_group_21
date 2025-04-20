/* xenia_vga.h - you like breaking userspace don't you */

#ifndef _XENIA_VGA_H
#define _XENIA_VGA_H

#ifndef ASM

#include "types.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// array storing an image already formatted in VGA text mode video memory format
// (i.e. character followed by color attribute byte per pixel)
extern uint16_t xenia_vga[VGA_HEIGHT][VGA_WIDTH];

#endif // ASM
#endif // _XENIA_VGA_H
