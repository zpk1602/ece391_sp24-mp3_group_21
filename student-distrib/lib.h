/* lib.h - Defines for useful library functions
 * vim:ts=4 noexpandtab
 */

#ifndef _LIB_H
#define _LIB_H

#include "types.h"

int32_t printf(int8_t *format, ...);
/* putc is in terminal.c for checkpoint 5 */
// void putc(uint8_t c);
int32_t puts(int8_t *s);
int8_t *itoa(uint32_t value, int8_t* buf, int32_t radix);
int8_t *strrev(int8_t* s);
uint32_t strlen(const int8_t* s);
void clear(void);
void test_interrupts(void);
void bksp(void);
void clear_screen(void);
void update_cursor(int row, int col);

void* memset(void* s, int32_t c, uint32_t n);
void* memset_word(void* s, int32_t c, uint32_t n);
void* memset_dword(void* s, int32_t c, uint32_t n);
void* memcpy(void* dest, const void* src, uint32_t n);
void* memmove(void* dest, const void* src, uint32_t n);
int32_t strncmp(const int8_t* s1, const int8_t* s2, uint32_t n);
int8_t* strcpy(int8_t* dest, const int8_t*src);
int8_t* strncpy(int8_t* dest, const int8_t*src, uint32_t n);

/* Userspace address-check functions */
int32_t bad_userspace_addr(const void* addr, int32_t len);
int32_t safe_strncpy(int8_t* dest, const int8_t* src, int32_t n);

/* Port read functions */
/* Inb reads a byte and returns its value as a zero-extended 32-bit
 * unsigned int */
static inline uint32_t inb(port) {
    uint32_t val;
    asm volatile ("             \n\
            xorl %0, %0         \n\
            inb  (%w1), %b0     \n\
            "
            : "=a"(val)
            : "d"(port)
            : "memory"
    );
    return val;
}

/* Reads two bytes from two consecutive ports, starting at "port",
 * concatenates them little-endian style, and returns them zero-extended
 * */
static inline uint32_t inw(port) {
    uint32_t val;
    asm volatile ("             \n\
            xorl %0, %0         \n\
            inw  (%w1), %w0     \n\
            "
            : "=a"(val)
            : "d"(port)
            : "memory"
    );
    return val;
}

/* Reads four bytes from four consecutive ports, starting at "port",
 * concatenates them little-endian style, and returns them */
static inline uint32_t inl(port) {
    uint32_t val;
    asm volatile ("inl (%w1), %0"
            : "=a"(val)
            : "d"(port)
            : "memory"
    );
    return val;
}

/* Writes a byte to a port */
#define outb(data, port)                \
do {                                    \
    asm volatile ("outb %b1, (%w0)"     \
            :                           \
            : "d"(port), "a"(data)      \
            : "memory", "cc"            \
    );                                  \
} while (0)

/* Writes two bytes to two consecutive ports */
#define outw(data, port)                \
do {                                    \
    asm volatile ("outw %w1, (%w0)"     \
            :                           \
            : "d"(port), "a"(data)      \
            : "memory", "cc"            \
    );                                  \
} while (0)

/* Writes four bytes to four consecutive ports */
#define outl(data, port)                \
do {                                    \
    asm volatile ("outl %l1, (%w0)"     \
            :                           \
            : "d"(port), "a"(data)      \
            : "memory", "cc"            \
    );                                  \
} while (0)

/* Clear interrupt flag - disables interrupts on this processor */
#define cli()                           \
do {                                    \
    asm volatile ("cli"                 \
            :                           \
            :                           \
            : "memory", "cc"            \
    );                                  \
} while (0)

/* Save flags and then clear interrupt flag
 * Saves the EFLAGS register into the variable "flags", and then
 * disables interrupts on this processor */
#define cli_and_save(flags)             \
do {                                    \
    asm volatile ("                   \n\
            pushfl                    \n\
            popl %0                   \n\
            cli                       \n\
            "                           \
            : "=r"(flags)               \
            :                           \
            : "memory", "cc"            \
    );                                  \
} while (0)

/* Set interrupt flag - enable interrupts on this processor */
#define sti()                           \
do {                                    \
    asm volatile ("sti"                 \
            :                           \
            :                           \
            : "memory", "cc"            \
    );                                  \
} while (0)

/* Restore flags
 * Puts the value in "flags" into the EFLAGS register.  Most often used
 * after a cli_and_save_flags(flags) */
#define restore_flags(flags)            \
do {                                    \
    asm volatile ("                   \n\
            pushl %0                  \n\
            popfl                     \n\
            "                           \
            :                           \
            : "r"(flags)                \
            : "memory", "cc"            \
    );                                  \
} while (0)

/* Log message
 * Logs a message using printf, showing the function, file, and line number
 * Inputs: The same as printf, i.e. string to print followed by args
 *         for each format string in the message
 * Outputs / Return value: none
 * Side effects: Prints to the screen */
#define log_msg(msg, ...) printf("[INFO] %s at %s:%d: " msg "\n",     \
        __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)

/* Panic
 * Currently disables hardware interrupts, and then halt loops
 * Inputs: none
 * Outputs / Return value: never returns
 * Side effects: freezes the CPU */
#define panic() do { asm volatile("cli"); while(1) asm volatile("hlt"); } while(0)

/* Panic with message
 * Prints out a message with similar args to printf (i.e. supporting format strings),
 * and then panics
 * Inputs: The same as printf, i.e. string to print followed by args
 *         for each format string in the message
 * Outputs / Return value: never returns
 * Side effects: prints to the screen, then freezes the CPU */
#define panic_msg(msg, ...)                                            \
do {                                                                   \
    printf("[PANIC] %s at %s:%d: " msg "\n", __FUNCTION__, __FILE__,   \
            __LINE__, ##__VA_ARGS__);                                  \
    panic();                                                           \
} while(0)

/* proper modulo operator, instead of just the signed remainder.
 * will always return the number r such that 0 <= r < b and a-r is a multiple of b */
static inline int floor_mod(int a, int b) {
    int res = a % b;
    if(b < 0) b = -b;
    return res < 0 ? res+b : res;
}
/* computes the floor of the floating point division of a/b, i.e.
 * it always rounds down, even for negative numbers. regular C division rounds to zero */
static inline int floor_div(int a, int b) {
    return (a - floor_mod(a,b)) / b;
}

#endif /* _LIB_H */
