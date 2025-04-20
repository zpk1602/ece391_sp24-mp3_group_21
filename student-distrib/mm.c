/* mm.c - Defines functions for handling memory management and paging.
 */

#include "mm.h"
#include "lib.h"
#include "x86_desc.h"
#include "syscall.h"
#include "process.h"
#include "terminal.h"

#define VIDEO 0xB8000
#define USER_MEM 0x8000000

/* Statically allocated arrays of page directory / table entries aligned to page boundaries,
 * used as the initial page directory and low page table for the kernel */
/* since these are initialized to all zeros, every entry will be not present */
page_dir_t kernel_page_dir;
page_tbl_t low_page_table;
page_tbl_t user_vidmap_page_table;

/* void paging_init(void)
 * Sets up initial page directories and tables, and turns on paging in the CPU
 * Inputs / Outputs / Return value: none
 * Side effects: Enables paging; memory accesses outside of pages after this
 *               function finishes will cause a paging fault!
 */
void paging_init(void) {
    pd_ent_t pd_ent;
    pt_ent_t pt_ent;
    cr0_t cr0;
    cr3_t cr3;
    cr4_t cr4;

    /* setup low memory page directory entry (where video memory is) 0x0 - 0x3FFFFF */
    pd_ent.val = 0; /* zero initialize reserved fields */
    pd_ent.present = 1;
    pd_ent.write_enable = 1;
    pd_ent.user_access = 0;
    pd_ent.write_through = 0;
    pd_ent.cache_disable = 0;
    pd_ent.accessed = 0;
    pd_ent.page_size = 0;
    /* the global flag here does not do anything, only the individual page table
     * entry global flags impact TLB behaviour */
    pd_ent.global = 1;
    pd_ent.avail = 0;
    pd_ent.base = ((uint32_t) &low_page_table) >> 12;
    kernel_page_dir[0] = pd_ent;

    /* setup kernel memory 4MiB page, from 0x400000 to 0x7FFFFF */
    pd_ent.val = 0; /* zero initialize reserved fields */
    pd_ent.present = 1;
    pd_ent.write_enable = 1;
    pd_ent.user_access = 0;
    pd_ent.write_through = 0;
    pd_ent.cache_disable = 0;
    pd_ent.accessed = 0;
    pd_ent.dirty = 0;
    pd_ent.page_size = 1;
    pd_ent.global = 1;
    pd_ent.avail = 0;
    pd_ent.page_attr_idx = 0;
    pd_ent.base_4m = 1;
    kernel_page_dir[1] = pd_ent;

    /* setup user memory page*/
    pd_ent.val = 0; /* zero initialize reserved fields */
    pd_ent.present = 1;
    pd_ent.write_enable = 1;
    pd_ent.user_access = 1;
    pd_ent.write_through = 0;
    pd_ent.cache_disable = 0;
    pd_ent.accessed = 0;
    pd_ent.dirty = 0;
    pd_ent.page_size = 1;
    pd_ent.global = 0;
    pd_ent.avail = 0;
    pd_ent.page_attr_idx = 0;
    pd_ent.base_4m = USER_MEM >> 22;
    kernel_page_dir[32] = pd_ent; // At 128 MB

    /* setup video memory 4KiB page, from VIDEO to VIDEO+PAGE_SIZE-1 */
    /* technically this only covers 4KiB of the 128KiB total of video memory,
     * but we only need 4000 = 80*25*2 bytes for the text mode we use.
     * for reference, video memory normally extends from 0xA0000 to 0xBFFFF */
    pt_ent.val = 0; /* zero initialize reserved fields */
    pt_ent.present = 1;
    pt_ent.write_enable = 1;
    pt_ent.user_access = 0;
    pt_ent.write_through = 1; /* make sure writes get sent out to VGA */
    pt_ent.cache_disable = 0;
    pt_ent.accessed = 0;
    pt_ent.page_attr_idx = 0;
    pt_ent.global = 1;
    pt_ent.avail = 0;
    pt_ent.base = VIDEO >> 12;
    low_page_table[VIDEO >> 12] = pt_ent;

    /* setup terminal video pages
     * double buffer: 0xB9000 - 0xB9FFF
     * terminal 0: 0xBA000 - 0xBAFFF
     * terminal 1: 0xBB000 - 0xBBFFF
     * terminal 2: 0xBC000 - 0xBCFFF */
    int i;
    for(i = 0; i < NUM_TERMINALS+1; ++i) {
        pt_ent.val = 0;
        pt_ent.present = 1;
        pt_ent.write_enable = 1;
        pt_ent.user_access = 0;
        pt_ent.write_through = 1; /* make sure writes get sent out to VGA */
        pt_ent.cache_disable = 0;
        pt_ent.accessed = 0;
        pt_ent.page_attr_idx = 0;
        pt_ent.global = 1;
        pt_ent.avail = 0;
        pt_ent.base = (VIDEO >> 12) + i + 1;
        low_page_table[(VIDEO >> 12) + i + 1] = pt_ent;
    }

    /* setup user vidmap memory page directory entry */
    pd_ent.val = 0; /* zero initialize reserved fields */
    pd_ent.present = 1;
    pd_ent.write_enable = 1;
    pd_ent.user_access = 1;
    pd_ent.write_through = 0;
    pd_ent.cache_disable = 0;
    pd_ent.accessed = 0;
    pd_ent.page_size = 0;
    /* the global flag here does not do anything, only the individual page table
     * entry global flags impact TLB behaviour */
    pd_ent.global = 1;
    pd_ent.avail = 0;
    pd_ent.base = ((uint32_t) &user_vidmap_page_table) >> 12;
    kernel_page_dir[USER_VIDMAP >> 22] = pd_ent;

    /* setup user video memory 4KiB page */
    pt_ent.val = 0; /* zero initialize reserved fields */
    pt_ent.present = 0; /* initially disabled, gets enabled by vidmap syscall */
    pt_ent.write_enable = 1;
    pt_ent.user_access = 1;
    pt_ent.write_through = 1; /* make sure writes get sent out to VGA */
    pt_ent.cache_disable = 0;
    pt_ent.accessed = 0;
    pt_ent.page_attr_idx = 0;
    pt_ent.global = 0;
    pt_ent.avail = 0;
    pt_ent.base = VIDEO >> 12;
    user_vidmap_page_table[(USER_VIDMAP & (PAGE_4M_SIZE-1)) >> 12] = pt_ent;

    cr0 = read_cr0();
    cr3 = read_cr3();
    cr4 = read_cr4();
    cr4.page_size_ext = 1;
    cr3.page_dir_base = ((uint32_t) &kernel_page_dir) >> 12;
    cr0.paging = 1;
    write_cr3(cr3);
    write_cr4(cr4);
    /* make sure we setup the flags and page directory before enabling paging bit in CR0 */
    write_cr0(cr0);
    /* page global enable has to be set after paging is enabled (x86 ISA manual,
     * vol 3, section 2.5) */
    cr4.page_global_enable = 1;
    write_cr4(cr4);

    printf("cr0: %#x cr2: %#x cr3: %#x cr4: %#x\n",
            read_cr0().val, read_cr2().val, read_cr3().val, read_cr4().val);
}



/* void set_user_page(int32_t pid)
 * Sets up the page directory entry for user memory
 * Inputs: pid - the process's pid to get user page info from
 * Outputs: none
 * Return value: none
 * Side effects: Changes the page directory entry for user memory
 */
void set_user_page(uint32_t pid) {
    uint32_t flags;
    cli_and_save(flags);
    pcb_t *pcb = pid_to_pcb(pid);
    int32_t address =  (pid + 2)  * PAGE_4M_SIZE; //TODO: make as a macro later
    kernel_page_dir[32].base_4m = address >> 22;  // At 128 MB virtual memory
    pt_ent_t *user_vidmap_pt_ent = &user_vidmap_page_table[
            (USER_VIDMAP & (PAGE_4M_SIZE-1)) >> 12];
    user_vidmap_pt_ent->present = pcb->vidmap;
    // user_vidmap_pt_ent->base = pcb->terminal_id == get_active_terminal_id() ?
    //         VIDEO >> 12 : (VIDEO >> 12) + pcb->terminal_id + 1; // 4KB index of vidmem page
    user_vidmap_pt_ent->base = (VIDEO >> 12) + pcb->terminal_id + 2; // 4KB index of vidmem page
    // need interrupts disabled because an interrupt between this read and write would be bad.
    // plus between the above and below, yeah
    write_cr3(read_cr3());
    restore_flags(flags);
}

/* syscall_vidmap
 * Enables the video memory user page.
 * Outputs: screen_start/arg1 - pointer to where the video memory address should be stored.
 * Return value: -1 on error, 0 on success.
 * Side effects: Maps the user video memory page. */
int32_t syscall_vidmap(int32_t arg1, int32_t arg2, int32_t arg3) {
    uint8_t **screen_start = (uint8_t**) arg1;
    if(!screen_start) return -1;
    if(check_user_bounds(screen_start, sizeof(uint8_t*))) return -1;
    // set the screen_start first, so that if we return early at this point, it'll be set
    *screen_start = (uint8_t*) USER_VIDMAP;

    pcb_t *pcb = get_current_pcb();
    if(pcb->vidmap) return 0; // it's already mapped
    // TODO: check if the above behaviour for a double vidmap call is correct
    uint32_t flags;
    cli_and_save(flags);
    pcb->vidmap = 1;
    set_user_page(pcb_to_pid(pcb));
    restore_flags(flags);
    return 0;
}

/* check_user_bounds
 * Checks that the provided buffer fits entirely within the virtual user page.
 * Inputs: buf - Pointer to the start of the buffer. (i.e. smallest address in it)
 *         len - The length of the buffer, in bytes.
 * Returns: 0 if it does fit, -1 if not
 * Side effects + Outputs: none
 * Time complexity: Constant */
int32_t check_user_bounds(const void *buf, uint32_t len) {
    uint32_t buf_int = (uint32_t) buf;
    if(buf_int < USER_VMEM_START) return -1;
    if(buf_int > USER_VMEM_END) return -1;
    uint32_t max_len = USER_VMEM_END - buf_int;
    return len <= max_len ? 0 : -1;
}

/* check_user_str_bounds
 * Checks that there exists a null terminated string entirely in the user page at the given
 * address that is no longer than max_len.
 * Inputs: str - Pointer to the user space C string.
 *         max_len - The maximum length that the string can be, excluding the null
 *                   terminator.
 * Returns: 0 on success, -1 on outside of user page, -2 on buffer exceeds max_len
 * Side effects + Outputs: none
 * Time complexity: Linear in the length of the string */
int32_t check_user_str_bounds(const uint8_t *str, uint32_t max_len) {
    if((uint32_t) str < USER_VMEM_START) return -1;
    int i;
    for(i = 0; i < max_len+1; ++i, ++str) {
        if((uint32_t) str >= USER_VMEM_END) return -1;
        if(*str == 0) return 0;
    }
    return -2;
}
