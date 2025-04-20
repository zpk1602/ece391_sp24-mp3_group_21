/* mm.h - Declares functions and types for handling memory management and paging.
 */

#ifndef _MM_H
#define _MM_H

#include "types.h"

/* 4KiB page size in x86, the amount of memory a single pt_ent covers, 1<<12 */
#define PAGE_SIZE (1<<12)
/* 4MiB large page size, the amount of memory a single pd_ent covers, 1<<22 */
#define PAGE_4M_SIZE (1<<22)
/* how many entries per page table/directory, 1<<10, each 4-bytes for a total of 4KiB */
#define PAGE_TBL_LEN (1<<10)

/* The start of the user page in virtual memory. */
#define USER_VMEM_START 0x08000000
/* The end of the user page in virtual memory. */
#define USER_VMEM_END (USER_VMEM_START+PAGE_4M_SIZE)

/* Virtual address of the start of the user video memory 4Kb page.
 * Can't find any information on what this value should be, so just set it
 * to an arbitrary value past the user page. */
#define USER_VIDMAP 0x09000000

/* the following structs come from the x86 ISA manual vol 3 section 3.7.6,
 * "Page-Directory and Page-Table Entries" */
/* page directory entry */
typedef struct pd_ent_t {
    union {
        uint32_t val;
        /* page directory entry pointing to a page table */
        struct __attribute__((packed)) {
            uint32_t present : 1;
            uint32_t write_enable : 1;
            uint32_t user_access : 1;
            uint32_t write_through : 1;
            uint32_t cache_disable : 1;
            uint32_t accessed : 1;
            uint32_t reserved_6 : 1;
            uint32_t page_size : 1; /* 1 for 4 MiB page, 0 for page table pointer */
            uint32_t global : 1;
            uint32_t avail : 3;
            uint32_t base : 20;
        };
        /* page directory entry pointing to a 4MiB page */
        struct __attribute__((packed)) {
            uint32_t pad_5_0 : 6; /* these are the same as for page table pointers */
            uint32_t dirty : 1;
            uint32_t pad_11_7 : 5; /* these are the same as for page table pointers */
            uint32_t page_attr_idx : 1;
            uint32_t reserved_21_13 : 9;
            uint32_t base_4m : 10;
        };
        /* not present page directory entry, present must be 0 */
        struct __attribute__((packed)) {
            uint32_t pad_0 : 1;
            uint32_t avail_empty : 31;
        };
    };
} pd_ent_t;

/* page table entry */
typedef struct pt_ent_t {
    union {
        uint32_t val;
        struct __attribute__((packed)) {
            uint32_t present : 1;
            uint32_t write_enable : 1;
            uint32_t user_access : 1;
            uint32_t write_through : 1;
            uint32_t cache_disable : 1;
            uint32_t accessed : 1;
            uint32_t dirty : 1;
            uint32_t page_attr_idx : 1;
            uint32_t global : 1;
            uint32_t avail : 3;
            uint32_t base : 20;
        };
        /* not present page table entry, present must be 0 */
        struct __attribute__((packed)) {
            uint32_t pad_0 : 1;
            uint32_t avail_empty : 31;
        };
    };
} pt_ent_t;

/* these alignment attributes apply to the whole table array type declaration, not the
 * individual entries */
typedef pd_ent_t __attribute((aligned(PAGE_SIZE))) page_dir_t[PAGE_TBL_LEN];
typedef pt_ent_t __attribute((aligned(PAGE_SIZE))) page_tbl_t[PAGE_TBL_LEN];

extern page_dir_t kernel_page_dir;
extern page_tbl_t low_page_table;
extern page_tbl_t user_vidmap_page_table;

extern void paging_init(void);
extern void set_user_page(uint32_t pid);

extern int32_t check_user_bounds(const void *buf, uint32_t len);
extern int32_t check_user_str_bounds(const uint8_t *str, uint32_t max_len);

#endif /* _MM_H */
