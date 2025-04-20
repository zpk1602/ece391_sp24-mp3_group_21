#ifndef _PIT_H
#define _PIT_H


#include "types.h"
#include "idt.h"
#include "lib.h"

#define PIT_IRQ 0
#define PIT_CMD_PORT 0x43
#define PIT_DATA_PORT 0x40

#define PIT_FREQ 1193182 // base frequency
#define PIT_MAX_FREQ 50 // max frequency
#define PIT_DEFAULT_TIME (1193182 / 50) // default frequency

extern volatile int enable_pit_test;

void pit_init();

// void pit_setrate(uint32_t rate);

#endif

