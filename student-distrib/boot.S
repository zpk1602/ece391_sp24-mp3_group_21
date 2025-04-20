# boot.S - start point for the kernel after GRUB gives us control
# vim:ts=4 noexpandtab

#define ASM     1

#include "multiboot.h"
#include "x86_desc.h"

.text

    # Multiboot header (required for GRUB to boot us)
    .long MULTIBOOT_HEADER_MAGIC
    .long MULTIBOOT_HEADER_FLAGS
    .long -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

# Entrypoint to the kernel
# When the bootloader hands control to the kernel, it jumps to this location to begin execution. 
.globl start, _start

.align 4 
# code is going to be 4 byte aligned (probably required by multiboot spec)
# read through the multiboot spec, can't find anything requiring the start symbol to be
# aligned, maybe it's required just by the ELF format? the multiboot header definitely
# does need to be aligned however, to a 4-byte boundary (guaranteed since the header
# comes first in the .text section for our kernel)

# GDB won't let you break at start, break at the continue label instead, two instructions later
/* start
 * The starting point for the whole kernel, sets up gdt, stack, and segments, and loads idtr
 * before calling into the C entry function in kernel.c.
 * Inputs: EAX -- multiboot magic number
 *         EBX -- pointer to multiboot info struct
 * Output / Ret val: This better not return lmao
 * Side effects: Affects memory, GDTR, IDTR, segment registers */
start:
_start:
    # Make sure interrupts are off
    cli
    jmp     continue

continue:
    # Load the GDT 
    # Your code here! :) 6.1.2**
    lgdt    gdt_desc
    
    # loading IDT
    lidt    idt_desc_ptr

    # Load CS with the new descriptor value
    ljmp    $KERNEL_CS, $keep_going

keep_going:
    # Set up ESP so we can have an initial stack
    movl    $0x800000, %esp

    # Set up the rest of the segment selector registers
    movw    $KERNEL_DS, %cx
    movw    %cx, %ss
    movw    %cx, %ds
    movw    %cx, %es
    movw    %cx, %fs
    movw    %cx, %gs

    # Push the parameters that entry() expects (see kernel.c):
    # eax = multiboot magic (some sort of magic number)
    # ebx = address of multiboot info struct 
    pushl   %ebx
    pushl   %eax

    # Jump to the C entrypoint to the kernel.
    call    entry

    # We'll never get back here, but we put in a hlt anyway. doesn't hurt
halt: 
    hlt
    jmp     halt
