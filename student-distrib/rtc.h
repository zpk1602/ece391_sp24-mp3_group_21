/* rtc.h -- definitions for the Real Time Clock driver */

#ifndef _RTC_H
#define _RTC_H

#include "x86_desc.h"
#include "fd.h"

#ifndef ASM

#define   RTC_IRQ 8
#define   RTC_ADDR 0x70
#define   RTC_DATA 0x71
#define   RTC_MASK_NMI 0x80
/* rate setting */
#define   RTC_REG_A 0xA
/* enable/disable interrupts, plus other flags */
#define   RTC_REG_B 0xB
/* signals end of interrupt */
#define   RTC_REG_C 0xC

volatile int rtc_int_flag;

/* typedef struct rtc_reg_a_t {
    union {
        uint8_t val;
        struct __attribute__((packed)) {
            uint32_t rate : 4;
            uint32_t div : 3;
            uint32_t uip : 1;
        };
    };
} rtc_reg_a_t;

typedef struct rtc_reg_b_t {
    union {
        uint8_t val;
        struct __attribute__((packed)) {
            uint32_t daylight_savings_en : 1;
            uint32_t hr24 : 1;
            uint32_t binary : 1;
            uint32_t sq_wave_en : 1;
            uint32_t update_end_int_en : 1;
            uint32_t TODO fill this out if we need it later
        };
    };
} rtc_reg_b_t; */

extern int enable_rtc_test;

void rtc_init();

int32_t rtc_setrate(uint32_t rate);

extern fd_driver_t rtc_fd_driver;

extern fd_open_t rtc_open;
extern fd_close_t rtc_close;
extern fd_read_t rtc_read;
extern fd_write_t rtc_write;


#endif /* ASM */
#endif /* _RTC_H */

