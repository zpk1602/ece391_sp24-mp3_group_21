#include "tests.h"
#include "x86_desc.h"
#include "lib.h"
#include "idt.h"
#include "rtc.h"
#include "mm.h"
#include "i8259.h"
#include "terminal.h"
#include "fs.h"
#include "process.h"
#include "syscall.h"
#include "pit.h"

#define PASS 1
#define FAIL 0

/* format these macros as you see fit */
#define TEST_HEADER 	\
	printf("[TEST %s] Running %s at %s:%d\n", __FUNCTION__, __FUNCTION__, __FILE__, __LINE__)
#define TEST_OUTPUT(name, result)	\
	printf("[TEST %s] Result = %s\n", name, (result) ? "PASS" : "FAIL");

static inline void assertion_failure(){
	/* Use exception #15 for assertions, otherwise
	   reserved by Intel */
	asm volatile("int $15");
}


/* Checkpoint 1 tests */

/* IDT Test - Example
 * 
 * Asserts that first 10 IDT entries are not NULL
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: None
 * Coverage: Load IDT, IDT definition
 * Files: x86_desc.h/S, idt.c/h
 */
int idt_test(){
	TEST_HEADER;

	int i;
	int result = PASS;
	for (i = 0; i < 10; ++i){
		if ((idt[i].offset_15_00 == NULL) && 
			(idt[i].offset_31_16 == NULL)){
			assertion_failure();
			result = FAIL;
		}
	}

	return result;
}

// add more tests here

/* Full IDT Test - Example
 * 
 * Asserts that the required entries are correct, and that the IDTR points to the idt
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: None
 * Coverage: Load IDT, IDT definition
 * Files: x86_desc.h/S, idt.c/h
 */
int full_idt_test() {
	TEST_HEADER;

	int i;
	idt_desc_t desc;
	desc.val[0] = KERNEL_CS << 16; desc.val[1] = 0x00008E00;
	idt_desc_t comp;
	/* check all 20 exceptions */
	for(i = 0; i < 20; i++) {
		comp = idt[i];
		if((comp.val[0] & 0xFFFF0000) != desc.val[0] ||
			(comp.val[1] & 0x0000FFFF) != desc.val[1])
			return FAIL;
		if(!(comp.val[0] & 0x0000FFFF)) return FAIL;
		if(!(comp.val[1] & 0xFFFF0000)) return FAIL;
	}
	/* check all 16 IRQ's */
	for(i = 0; i < 16; i++) {
		comp = idt[0x20 + i];
		if((comp.val[0] & 0xFFFF0000) != desc.val[0] ||
			(comp.val[1] & 0x0000FFFF) != desc.val[1])
			return FAIL;
		if(!(comp.val[0] & 0x0000FFFF)) return FAIL;
		if(!(comp.val[1] & 0xFFFF0000)) return FAIL;
	}
	/* check syscall idt entry */
	desc.val[1] = 0x0000EF00;
	comp = idt[0x80];
	if((comp.val[0] & 0xFFFF0000) != desc.val[0] ||
		(comp.val[1] & 0x0000FFFF) != desc.val[1])
		return FAIL;
	if(!(comp.val[0] & 0x0000FFFF)) return FAIL;
	if(!(comp.val[1] & 0xFFFF0000)) return FAIL;
	/* check all other entries */
	for(i = 20; i < 256; i++) {
		if(i >= 0x20 && i < 0x30) continue; /* skip IRQ's */
		if(i == 0x80) continue; /* skip syscall */
		comp = idt[i];
		/* should be all zeros */
		if(comp.val[0] || comp.val[1]) return FAIL;
	}

	x86_desc_t idtr;
	asm volatile("sidt (%0)" :: "r"(&idtr.size) : "memory", "cc");
	if(idtr.size != 8*256-1 || idtr.addr != (uint32_t)(&idt))
		return FAIL;

	return PASS;
}

/* RTC Main Test
 * 
 * Sets the RTC to repeatedly call test_interrupts() in lib.c
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: None
 * Coverage: RTC
 * Files: rtc.c/h, idtasm.S, idt.c/h
 */
int rtc_main_test() {
	TEST_HEADER;

	/* see rtc.c for what this does */
	enable_rtc_test = 1;

	return PASS;
}

/* Single generic interrupt test
 * Tests a single vector index to see if calling via int $n is able
 * to retain the value in ECX as well as ESP.
 * Inputs: intnum -- The IDT vector index to test.
 * Side effects: Causes surrounding function to return if the test failed.
 *               Triggers the interrupt number passed in using int $n. */
#define test_int(intnum) \
do { \
	uint32_t orig, next; \
	asm volatile("movl $-1, %%eax; movl %%esp, %%ecx; int %2; movl %%esp, %%edx" \
			: "=c"(orig), "=d"(next) : "i"(intnum) : "memory", "cc", "eax"); \
	if(orig != next) return FAIL; \
} while(0)

/* Generic Interrupt Test
 * 
 * Tests triggering the first ten interrupts, as well as the syscall interrupt, via
 * the int $n instruction.
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Triggers an interrupt to fire
 * Coverage: Interrupt Handling
 * Files: idtasm.S, idt.c/h
 */
int test_ints() {
	TEST_HEADER;
	test_int(0);
	// test_int(1);
	// test_int(2);
	// test_int(3);
	// test_int(4);
	// test_int(5);
	// test_int(6);
	// test_int(7);
	// test_int(8);
	// test_int(9);
	// test_int(10);
	// test_int(11);
	// test_int(12);
	// test_int(13);
	// test_int(14);
	// test_int(15);
	// test_int(16);
	// test_int(17);
	// test_int(18);
	// test_int(19);
	/* this one should not be present in the idt, causing a general protection
	 * exception, aka int $13 */
	// test_int(20);
	return PASS;
}

/* Syscall Interrupt Test
 * 
 * Tests triggering the syscall interrupt via
 * the int $n instruction.
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Triggers an interrupt to fire
 * Coverage: Interrupt Handling
 * Files: idtasm.S, idt.c/h
 */
int test_syscall_cp1() {
	TEST_HEADER;
	test_int(0x80);
	return PASS;
}

/* Divide by zero test
 *
 * Tests the dividing by zero handler.
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Triggers an exception to fire
 * Coverage: Interrupt Handling
 * Files: idtasm.S, idt.c/h
 */
int test_div_by_zero() {
	TEST_HEADER;
	volatile uint32_t whoa = 0;
	whoa = whoa / whoa;
	return FAIL;
}

/* Undefined opcode test
 *
 * Tests running an undefined opcode.
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Triggers an exception to fire
 * Coverage: Interrupt Handling
 * Files: idtasm.S, idt.c/h
 */
int test_undef_opcode() {
	TEST_HEADER;
	asm volatile("ud2");
	return FAIL;
}

/* Out of bounds vector indices
 *
 * Tests calling various functions with out of bounds indices
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Panics
 * Coverage: Interrupt Handling
 * Files: idt.c/h
 */
int test_idt_oob() {
	TEST_HEADER;
	iret_context_base_t whatever;
	irq_handler(16, &whatever);
	// irq_handler(3, &whatever);
	// exception_handler_all(20, &whatever);
	// enable_irq(16, &whatever);
	// disable_irq(16, &whatever);
	// send_eoi(16, &whatever);
	return FAIL;
}

/* Out of bounds vector indices irq_register_handler
 *
 * Tests calling irq_register_handler with out of bounds indices
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Panics
 * Coverage: Interrupt Handling
 * Files: idt.c/h
 */
int test_irq_reg_oob() {
	TEST_HEADER;
	irq_handler_node_t test_node = IRQ_HANDLER_NODE_INIT;
	test_node.handler = (irq_handler_t) 1; /* subvert null check */
	irq_register_handler(16, &test_node);
	return FAIL;
}

/* Null IRQ node test
 *
 * Tests calling irq_register_handler with a null node pointer
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Panics
 * Coverage: Interrupt Handling
 * Files: idt.c/h
 */
int test_irq_null_node() {
	TEST_HEADER;
	irq_register_handler(4, NULL);
	return FAIL;
}

/* Null IRQ handler test
 *
 * Tests calling irq_register_handler with a null handler pointer
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Panics
 * Coverage: Interrupt Handling
 * Files: idt.c/h
 */
int test_irq_null_handler() {
	TEST_HEADER;
	irq_handler_node_t node = IRQ_HANDLER_NODE_INIT;
	node.handler = NULL;
	irq_register_handler(4, &node);
	return FAIL;
}

/* Null IRQ double register test
 *
 * Tests calling irq_register_handler twice with the same node
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Panics
 * Coverage: Interrupt Handling
 * Files: idt.c/h
 */
int test_irq_double_reg() {
	TEST_HEADER;
	irq_handler_node_t node = IRQ_HANDLER_NODE_INIT;
	node.handler = (irq_handler_t) 0x1; /* subvert null check */
	irq_register_handler(4, &node);
	irq_register_handler(4, &node);
	return FAIL;
}

/* Already used node test
 *
 * Tests calling irq_register_handler with an already linked node
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Panics
 * Coverage: Interrupt Handling
 * Files: idt.c/h
 */
int test_irq_reg_in_use() {
	TEST_HEADER;
	irq_handler_node_t node = IRQ_HANDLER_NODE_INIT;
	node.handler = (irq_handler_t) 0x1;
	node.next = &node;
	irq_register_handler(4, &node);
	return FAIL;
}

/* IRQ without handlers test
 *
 * Tests firing an IRQ interrupt with no handlers registered
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Panics
 * Coverage: Interrupt Handling
 * Files: idt.c/h
 */
int test_irq_no_handlers() {
	TEST_HEADER;
	/* use IRQ 2 for tests since it's the cascade IRQ and shouldn't have any use */
	asm volatile("int $0x22");
	return FAIL;
}

/* A simple IRQ handler that never handles
 * Inputs: See irq_handler_t in idt.h
 * Return Value: Always IRQ_UNHANDLED
 * Side effects: none
 */
static int test_irq_unhandled_handler(uint32_t irq) {
	return IRQ_UNHANDLED;
}

/* unhandled IRQ test
 *
 * Tests firing an IRQ interrupt with only a handler that returns IRQ_UNHANDLED
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Panics
 * Coverage: Interrupt Handling
 * Files: idt.c/h
 */
int test_irq_unhandled() {
	TEST_HEADER;
	irq_handler_node_t node = IRQ_HANDLER_NODE_INIT;
	node.handler = &test_irq_unhandled_handler;
	irq_register_handler(2, &node);
	/* use IRQ 2 for tests since it's the cascade IRQ and shouldn't have any use */
	asm volatile("int $0x22");
	return FAIL;
}

/* Page directory/table test
 *
 * Tests that the page directory contains the right entries, and that the control
 * registers are set correctly.
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: Panics
 * Coverage: Interrupt Handling
 * Files: idt.c/h
 */
int page_dir_test() {
	TEST_HEADER;
	int i;
	pt_ent_t actual_pt;
	pd_ent_t actual_pd;
	for(i = 0; i < 1024; ++i) {
		if(i == (0xB8000 >> 12)) {
			actual_pt = low_page_table[i];
			if(actual_pt.base != (0xB8000 >> 12)) return FAIL;
			actual_pt.base = 0;
			actual_pt.accessed = 0;
			actual_pt.dirty = 0;
			if(actual_pt.val != 0x0000010B) return FAIL;
		} else {
			if(low_page_table[i].present) return FAIL;
		}
		if(i == 0) {
			actual_pd = kernel_page_dir[i];
			if(actual_pd.base != ((uint32_t)&low_page_table >> 12)) return FAIL;
			actual_pd.base = 0;
			actual_pd.accessed = 0;
			if(actual_pd.val != 0x00000103) return FAIL;
		} else if(i == 1) {
			actual_pd = kernel_page_dir[i];
			if(actual_pd.base_4m != 1) return FAIL;
			actual_pd.base_4m = 0;
			actual_pd.accessed = 0;
			actual_pd.dirty = 0;
			if(actual_pd.val != 0x00000183) return FAIL;
		} else {
			if(kernel_page_dir[i].present) return FAIL;
		}
	}
	cr0_t cr0 = read_cr0();
	if(!cr0.paging || !cr0.protected_mode) return FAIL;
	cr3_t cr3 = read_cr3();
	if(cr3.page_dir_base != ((uint32_t)&kernel_page_dir >> 12)) return FAIL;
	cr3.page_dir_base = 0;
	if(cr3.val != 0) return FAIL;
	cr4_t cr4 = read_cr4();
	if(!cr4.page_size_ext || !cr4.page_global_enable) return FAIL;

	return PASS;
}

/* CR2 read/write test
 *
 * Tests that we can read back a value written to CR2
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: Modifies CR2
 * Coverage: read/write_cr2
 * Files: x86_desc.h
 */
int cr2_rw_test(void) {
	TEST_HEADER;
	cr2_t orig, test, actual;
	orig = read_cr2();
	test.val = 0xBEEBD00B;
	write_cr2(test);
	actual = read_cr2();
	if(test.val != actual.val) {
		write_cr2(orig);
		return FAIL;
	}
	write_cr2(orig);
	return PASS;
}

/* scrolling_test
 *
 * Tests that character printing properly scrolls the screen.
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: prints to the screen using puts/putc
 * Coverage: puts() and putc() print location movement (wrapping and scrolling)
 * Files: lib.c/h
 */
int scrolling_test(void) {
	TEST_HEADER;

	/* test scrolling */
	int i;
	for(i = 0; i < 10; ++i) {
		puts("many short lines\n");
	}
	for(i = 0; i < 10; ++i) {
		puts("many other short lines\n");
	}
	/* test line wrap and scroll */
	for(i = 0; i < 15; ++i) {
		puts("123456");
	}
	putc('\n');

	return PASS;
}

/* paging_read_test
 *
 * Tests that reading from pages either works or faults correctly
 *   for a given address.
 * Inputs: ptr -- The pointer byte address to try reading
 * Outputs: none
 * Return value: none
 * Side effects: Causes a page fault if the ptr is a part of a non-present page.
 *               The value at the given address does not change.
 *               Can cause other effects if the address is an MMIO address.
 * Coverage: Paging initialization
 * Files: mm.h/c
 */
static void paging_read_test(uint8_t *ptr) {
	/* use assembly to make sure the instructions execute with nothing in between */
	asm volatile("movb (%0), %%al" :: "r"(ptr) : "eax", "memory");
}

/* paging_rw_test
 *
 * Tests that reading/writing from/to pages either works or faults correctly
 *   for a given address.
 * Inputs: ptr -- The pointer byte address to try accessing
 * Outputs: none
 * Return value: none
 * Side effects: Causes a page fault if the ptr is a part of a non-present or read-only page.
 *               The value at the given address does not change.
 *               Can cause other effects if the address is an MMIO address.
 * Coverage: Paging initialization
 * Files: mm.h/c
 */
static void paging_rw_test(uint8_t *ptr) {
	/* use assembly to make sure the instructions execute with nothing in between */
	asm volatile("movb (%0), %%al; movb %%al, (%0)" :: "r"(ptr) : "eax", "memory");
}

/* paging_no_fault_test
 *
 * Tests that no faults occur when reading/writing to addresses within mapped pages.
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: none
 * Coverage: Paging initialization
 * Files: mm.h/c
 */
int paging_no_fault_test() {
	TEST_HEADER;
	int i;
	if(!read_cr0().paging) return FAIL;
	/* start of video memory */
	paging_rw_test((uint8_t*) 0xB8000);
	/* end of video memory */
	paging_rw_test((uint8_t*) 0xB8FFF);
	/* check start/end of each 4KiB page in kernel memory */
	for(i = 0x400000; i < 0x800000; i += 0x1000) {
		/* start */
		paging_rw_test((uint8_t*) i);
		/* end */
		paging_rw_test((uint8_t*) i + 0xFFF);
	}
	return PASS;
}

/* paging_fault_test
 *
 * Tests that faults occur when reading/writing to addresses outside of mapped pages.
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: Page fault lol
 * Coverage: Paging initialization
 * Files: mm.h/c
 */
int paging_fault_test() {
	TEST_HEADER;
	/* writable pages must also be readable on x86, so we don't need to check
	 * if the page is writable */
	/* null pointer */
	paging_read_test((uint8_t*) 0x0);
	/* just before video memory */
	// paging_read_test((uint8_t*) 0xB7FFF);
	/* just after video memory */
	// paging_read_test((uint8_t*) 0xB9000);
	/* just before kernel memory */
	// paging_read_test((uint8_t*) 0x3FFFFF);
	/* just after kernel memory */
	// paging_read_test((uint8_t*) 0x800000);
	/* max uint32_t */
	// paging_read_test((uint8_t*) 0xFFFFFFFF);

	/* we should have faulted by now */
	return FAIL;
}





/* Checkpoint 2 tests */

/* terminal_read_test
 *
 * Tests that term_read handles the keyboard buffer correctly
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: none
 * Coverage: term_read clear_keyboard_buffer
 * Files: terminal.c/h keyboard.c/h
 */
int terminal_read_test() {
	TEST_HEADER;
	uint32_t flags;
	cli_and_save(flags);
	fd_info_t fd_info;
	uint8_t buffer[BUFFER_SIZE];
	memset(buffer, '\0', sizeof(buffer));
	int active_terminal_id = get_active_terminal_id();
	strcpy(terminals[active_terminal_id].keyboard_buffer, "Hello, world!\n");
	// 14 bytes because "Hello, world!\n" is 14 characters long
	terminals[active_terminal_id].buffer_index = 14; // simulate the buffer being filled (14 chars of "Hello, world!\n")
	terminals[active_terminal_id].term_in_flag = 1; // simulate enter key

	int32_t bytes_read = term_read(&fd_info, buffer, sizeof(buffer));


	// 14 bytes because "Hello, world!\n" is 14 characters long
	if (bytes_read != 14) {
		log_msg("terminal read failed: expected 14 bytes, got %d", bytes_read);
		restore_flags(flags);
		return FAIL;
	}
	int32_t i;
	uint8_t *actual = (uint8_t*) "Hello, world!\n";
	// 14 bytes because "Hello, world!\n" is 14 characters long
	for(i = 0; i < 14; i++) {
		if(buffer[i] != actual[i]) {
			log_msg("terminal read failed: expected 'Hello, world!\\n', got '%s'", buffer);
			restore_flags(flags);
			return FAIL;
		}
	}

	terminals[active_terminal_id].term_in_flag = 0; // test clean up
	clear_keyboard_buffer();
	restore_flags(flags);
	return PASS;
}

/* terminal_write_test
 *
 * Tests that term_write writes to the screen
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: writes to the screen
 * Coverage: term_write
 * Files: terminal.c/h keyboard.c/h
 */
int terminal_write_test() {
	TEST_HEADER;
	fd_info_t fd_info;
	uint8_t buffer[BUFFER_SIZE];
	strcpy((int8_t*) buffer, "Hello, world!\n");

	// 14 bytes because "Hello, world!\n" is 14 characters long
	int32_t bytes_written = term_write(&fd_info, buffer, 14);

	if(bytes_written != 14) { // 14 bytes because "Hello, world!\n" is 14 characters long
		log_msg("terminal write failed: expected 14 bytes, got %d\n", bytes_written);
		return FAIL;
	}
	
	return PASS;
}

/* terminal_open_test
 *
 * Tests that term_open works correctly
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: none
 * Coverage: term_open
 * Files: terminal.c/h keyboard.c/h
 */
int terminal_open_test() {
	TEST_HEADER;
	fd_info_t fd_info;
	if(term_open(&fd_info, NULL)) {
		return FAIL;
	}
	return PASS;
}

/* terminal_close_test
 *
 * Tests that term_close works correctly
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: none
 * Coverage: term_close
 * Files: terminal.c/h keyboard.c/h
 */
int terminal_close_test() {
	TEST_HEADER;
	fd_info_t fd_info;
	if(term_close(&fd_info)) {
		return FAIL;
	}
	return PASS;
}

/* term_read_write_test
 *
 * Tests that reading from and writing to the terminal driver works in a loop.
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: none
 * Coverage: term_read term_write
 * Files: terminal.c/h keyboard.c/h
 */
int term_read_write_test() {
	TEST_HEADER;
	fd_info_t fd_info;
	uint8_t buf[BUFFER_SIZE];
	if(term_open(&fd_info, NULL)) {
		return FAIL;
	}
	while(1) {
		int32_t count_read = fd_info.file_ops->read(&fd_info, buf, BUFFER_SIZE);
		if(count_read < 0) return FAIL;
		printf("count read: %d\nline: ", count_read);
		if(count_read != fd_info.file_ops->write(&fd_info, buf, count_read))
			return FAIL;
	}
}

/* fs_dir_fd_test
 *
 * Tests listing off file names similar to ls using directory_open and directory_read via
 * the directory_fd_driver jumptable.
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: Prints to the screen, one line per file
 * Coverage: directory_fd_driver
 * Files: fs.h/c fd.h
 */
int fs_dir_fd_test() {
	TEST_HEADER;
	fd_info_t dir_fd;
	if(file_open(&dir_fd, (uint8_t*) ".")) {
		log_msg("dir open!");
		return FAIL;
	}
	if(dir_fd.file_ops != &directory_fd_driver) {
		log_msg("file ops incorrect!");
		return FAIL;
	}
	uint8_t name_buf[FS_MAX_FNAME_LEN+10];
	memset(name_buf, '\0', FS_MAX_FNAME_LEN+1);
	while(1) {
		int32_t count = dir_fd.file_ops->read(&dir_fd, name_buf, FS_MAX_FNAME_LEN+10);
		if(count < 0) {
			log_msg("error reading file!");
			return FAIL;
		}
		if(count > FS_MAX_FNAME_LEN) return FAIL;
		if(count == 0) break;
		int32_t i;
		puts("filename: ");
		for(i = 0; i < count; i++) {
			putc(name_buf[i]);
		}
		putc('\n');
	}
	return PASS;
}

/* fs_dentry_index_test
 *
 * Tests listing off file info similar to ls using read_dentry_by_index.
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: Prints to the screen, one line per file
 * Coverage: read_dentry_by_index
 * Files: fs.h/c fd.h
 */
int fs_dentry_index_test() {
	TEST_HEADER;
	dentry_t dentry;
	uint8_t fname_buf[FS_MAX_FNAME_LEN+1];
	memset(fname_buf, '\0', FS_MAX_FNAME_LEN+1);
	uint32_t i;
	for(i = 0; i < fs_boot_blk->num_dentries; ++i) {
		if(read_dentry_by_index(i, &dentry)) {
			log_msg("read dentry failed");
			return FAIL;
		}
		uint32_t size = 0;
		if(dentry.type == 2) {
			/* regular file */
			if(dentry.inode >= fs_boot_blk->num_inode) {
				log_msg("inode out of range");
				return FAIL;
			}
			size = inode_start[dentry.inode].file_length;
		} else {
			if(dentry.inode != 0) {
				log_msg("inode nonzero for non-regular file!");
				return FAIL;
			}
		}
		memcpy(fname_buf, dentry.filename, FS_MAX_FNAME_LEN);
		printf("filename: %s type: %u inode: %u size: %u\n",
				fname_buf, dentry.type, dentry.inode, size);
	}
	return PASS;
}


/* rtc_openclose_test
 *
 * Tests opening and closing an RTC file descriptor, including fail conditions
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: Sets the rate of the RTC hardware interrupts
 * Coverage: rtc_open, rtc_close
 * Files: rtc.h/c fd.h
 */
int rtc_openclose_test() {
	TEST_HEADER;
	fd_info_t fd_info;
	if(rtc_open(NULL, (uint8_t*) "lol") != -1) {
		log_msg("rtc_open null fd_info failed");
		return FAIL;
	}
	if(rtc_open(&fd_info, NULL) != -1) {
		log_msg("rtc_open null filename failed");
		return FAIL;
	}
	int32_t result = rtc_open(&fd_info, (uint8_t*) "whatever");
	if (result != 0) {
		return FAIL;
	}
	if(rtc_close(NULL) != -1) {
		log_msg("rtc_close null fd_info failed");
		return FAIL;
	}
	result = rtc_close(&fd_info);
	if (result != 0) {
		return FAIL;
	}
	return PASS;
}

/* rtc_write_test
 *
 * Tests various fail conditions of rtc_write
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: Sets the rate of the RTC hardware interrupts
 * Coverage: rtc_write
 * Files: rtc.h/c fd.h
 */
int rtc_write_test() {
	TEST_HEADER;
	fd_info_t fd_info;
	rtc_open(&fd_info, (uint8_t*) "whatever");
	uint32_t rate;
	rate = 2;
	if(rtc_write(&fd_info, &rate, sizeof(rate)) != 4) {
		log_msg("rtc_write failed");
		return FAIL;
	}
	if(rtc_write(NULL, &rate, sizeof(rate)) != -1) {
		log_msg("rtc_write passed with null fd_info");
		return FAIL;
	}
	rate = 1;
	if(rtc_write(&fd_info, &rate, sizeof(rate)) != -1) {
		log_msg("rtc_write passed with rate 1hz");
		return FAIL;
	}
	rate = 300;
	if(rtc_write(&fd_info, &rate, sizeof(rate)) != -1) {
		log_msg("rtc_write passed with rate not power of 2");
		return FAIL;
	}
	rate = 2048;
	if(rtc_write(&fd_info, &rate, sizeof(rate)) != -1) {
		log_msg ("rtc_write passed with rate too large");
		return FAIL;
	}
	rate = 2;
	if(rtc_write(&fd_info, &rate, 2) != -1) {
		log_msg("rtc_write passed with nbytes not 4");
		return FAIL;
	}
	if(rtc_close(&fd_info) != 0) {
		log_msg("rtc_close failed");
		return FAIL;
	}
	return PASS;
}

/* rtc_read_test
 *
 * Tests waiting for a single interrupt using rtc_read, plus various fail conditions
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: Sets the rate of the RTC hardware interrupts
 * Coverage: rtc_read
 * Files: rtc.h/c fd.h
 */
int rtc_read_test() {
	TEST_HEADER;
	fd_info_t fd_info;
	uint32_t trash;
	if(rtc_open(&fd_info, (uint8_t*) "whatever")) return -1;
	// rtc_int_flag = 1; // raises the interrupt flag
	if(rtc_read(&fd_info, &trash, 4) != 0) {
		log_msg("rtc_read failed");
		return FAIL;
	}
	if(rtc_read(NULL, &trash, 4) != -1) {
		log_msg("rtc_read null fd_info failed");
		return FAIL;
	}
	if(rtc_read(&fd_info, NULL, 11111) != -1) {
		log_msg("rtc_read null buf failed");
		return FAIL;
	}
	if(rtc_read(&fd_info, &trash, -1) != -1) {
		log_msg("rtc_read negative nbytes failed");
		return FAIL;
	}
	// rtc_int_flag = 0; // lowers the interrupt flag
	if(rtc_close(&fd_info) != 0) {
		log_msg("rtc_close failed");
		return FAIL;
	}
	return PASS;
}


/* rtc_freq_test
 *
 * Tests the RTC driver by printing a character at a rate that increases exponentially
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: Prints to the screen, one character per interrupt, clears the screen
 * Coverage: rtc_read, rtc_write
 * Files: rtc.h/c fd.h
 */
int rtc_freq_test() {
	TEST_HEADER;
	fd_info_t fd_info;
	if(rtc_open(&fd_info, (uint8_t*) "whatever")) return FAIL;

	uint32_t rate; 
	int32_t print_char; 
	int32_t seconds;
	int32_t i;
	uint32_t trash;
	seconds = 4;

	for (rate = 2; rate <= 1024; rate *= 2) {
		clear();
		print_char = rate * seconds;
		if(rtc_write(&fd_info, &rate, sizeof(rate)) != 4) return FAIL;
		for(i = 0; i < print_char; i++) {
			if(rtc_read(&fd_info, &trash, 4) != 0) return FAIL;
			putc('1');
		}
	}
	putc('\n');
	if(rtc_close(&fd_info) != 0) return FAIL;
	return PASS;
}

/* rtc_open_rate_test
 *
 * Tests that the RTC drivers sets the rate to 2 Hz when opening the fd
 * Inputs: none; Outputs: none
 * Return value: PASS/FAIL
 * Side effects: Prints to the screen, one character per interrupt, clears the screen
 * Coverage: rtc_read, rtc_write
 * Files: rtc.h/c fd.h
 */
int rtc_open_rate_test() {
	TEST_HEADER;
	fd_info_t fd_info;
	if(rtc_open(&fd_info, (uint8_t*) "whatever")) return FAIL;
	uint32_t rate = 128;
	if(rtc_write(&fd_info, &rate, sizeof(rate)) != 4) return FAIL;
	if(rtc_close(&fd_info)) return FAIL;
	if(rtc_open(&fd_info, (uint8_t*) "whatever")) return FAIL;
	uint32_t trash;
	int i;
	for(i = 0; i < 10; ++i) {
		if(rtc_read(&fd_info, &trash, 4) != 0) return FAIL;
		putc('4');
	}
	if(rtc_close(&fd_info)) return FAIL;
	putc('\n');
	return PASS;
}

#define FILE_CHUNK_SIZE 32
#define FILE_START 0
#define FILE_END 1

/* fs_file_fd_generic_test
 * Tests reading from a file using file_read
 * Inputs: filename -- the name of the file to read from, as a null terminated string
 *         end -- boolean, whether or not to read from the start or end (0 = start, 1 = end)
 *         offset -- for end=0, offset from start of file to start reading from
 *                   for end=1, bytes between end of read data and end of file
 *         nchunks -- the number of FILE_CHUNK_SIZE chunks to read from the file
 * Outputs: none
 * Return value: PASS/FAIL
 * Side effects: writes to the screen the contents specified.
 * Coverage: file_fd_driver, file_read, read_data
 * Files: fs.c/h, fd.h */
static int fs_file_fd_generic_test(const uint8_t *filename, int end, uint32_t offset,
		uint32_t nchunks) {
	dentry_t dentry;
	uint32_t size, nbytes;
	if(read_dentry_by_name(filename, &dentry)) return FAIL;
	if(dentry.type != FS_DENTRY_FILE) return FAIL;
	if(end) {
		/* overall, the idea of this code is to do the following:
		 * offset = size - offset - nchunks * FILE_CHUNK_SIZE
		 * but using saturating operations, ie if anything would overflow,
		 * just set offset to 0 */
		size = inode_start[dentry.inode].file_length;
		if(size <= offset) {
			offset = 0;
		} else {
			offset = size - offset;
			/* check the top 5 bits, if any are set, then multiplying by 32 (shifting
			 * to the left by 5) will cause an overflow, and we should just use the
			 * max uint32_t value, otherwise multiply as usual. */
			nbytes = (nchunks & 0xF8000000) ? 0xFFFFFFFF : nchunks * FILE_CHUNK_SIZE;
			offset = nbytes >= offset ? 0 : offset - nbytes;
		}
	}
	int32_t count;
	uint8_t buf[FILE_CHUNK_SIZE];
	fd_info_t fd_info;
	if(file_fd_driver.open(&fd_info, filename)) return FAIL;
	fd_info.file_pos = offset;
	uint32_t chunk_idx = 0;
	while((count = fd_info.file_ops->read(&fd_info, buf, FILE_CHUNK_SIZE)) > 0 &&
			chunk_idx < nchunks) {
		int i;
		for(i = 0; i < count; ++i) {
			if(buf[i] != 0) putc(buf[i]);
		}
		chunk_idx++;
	}
	if(count < 0) return FAIL;
	putc('\n');
	if(fd_info.file_ops->close(&fd_info)) return FAIL;
	return PASS;
}

/* fs_file_fd_generic_test
 * Tests reading from a file using file_read, using a variety of inputs. (Uncomment the
 * specific test to run.)
 * Inputs: none
 * Outputs: none
 * Return value: PASS/FAIL
 * Side effects: writes to the screen the contents specified.
 * Coverage: file_fd_driver, file_read, read_data
 * Files: fs.c/h, fd.h */
int fs_file_fd_test() {
	TEST_HEADER;
	return fs_file_fd_generic_test((uint8_t*) "frame0.txt", FILE_START, 0, 0xFFFFFFFF);
	// return fs_file_fd_generic_test((uint8_t*) "frame0.txt", FILE_START, 23, 0xFFFFFFFF);
	// return fs_file_fd_generic_test((uint8_t*) "frame1.txt", FILE_START, 0, 0xFFFFFFFF);
	// return fs_file_fd_generic_test((uint8_t*) "frame1.txt", FILE_START, 41, 0xFFFFFFFF);
	// return fs_file_fd_generic_test((uint8_t*) "grep", FILE_START, 0, 4);
	// return fs_file_fd_generic_test((uint8_t*) "grep", FILE_START, 0, 0xFFFFFFFF);
	// return fs_file_fd_generic_test((uint8_t*) "grep", FILE_END, 0, 10);
	// return fs_file_fd_generic_test((uint8_t*) "ls", FILE_START, 0, 4);
	// return fs_file_fd_generic_test((uint8_t*) "ls", FILE_START, 0, 0xFFFFFFFF);
	// return fs_file_fd_generic_test((uint8_t*) "ls", FILE_END, 0, 10);
	// return fs_file_fd_generic_test((uint8_t*) "fish", FILE_START, 0, 4);
	// return fs_file_fd_generic_test((uint8_t*) "fish", FILE_START, 0, 0xFFFFFFFF);
	// return fs_file_fd_generic_test((uint8_t*) "fish", FILE_END, 0, 10);
	// return fs_file_fd_generic_test((uint8_t*) "verylargetextwithverylongname.tx",
	// 		FILE_END, 0, 10);
	// return fs_file_fd_generic_test((uint8_t*) "verylargetextwithverylongname.tx",
	// 		FILE_START, 0, 10);
	// return fs_file_fd_generic_test((uint8_t*) "verylargetextwithverylongname.tx",
	// 		FILE_START, 4080, 10);
}

/* fs_fail_conditions
 * Tests various fail conditions of various filesystem functions
 * Inputs: none
 * Outputs: none
 * Return value: PASS/FAIL
 * Side effects: none
 * Coverage: every function in fs.c
 * Files: fs.c/h, fd.h */
int fs_fail_conditions() {
	TEST_HEADER;
	dentry_t dentry;
	/* read_dentry_by_name tests */
	if(read_dentry_by_name(NULL, &dentry) != -1) {
		log_msg("read_dentry_by_name null filename failed");
		return FAIL;
	}
	if(read_dentry_by_name((uint8_t*) "lol", NULL) != -1) {
		log_msg("read_dentry_by_name null dentry failed");
		return FAIL;
	}
	if(read_dentry_by_name((uint8_t*) "lol", NULL) != -1) {
		log_msg("read_dentry_by_name null dentry failed");
		return FAIL;
	}
	if(read_dentry_by_name((uint8_t*) "lol", &dentry) != -1) {
		log_msg("read_dentry_by_name missing file failed");
		return FAIL;
	}
	if(read_dentry_by_name((uint8_t*) "", &dentry) != -1) {
		log_msg("read_dentry_by_name empty filename failed");
		return FAIL;
	}
	if(read_dentry_by_name((uint8_t*) "verylargetextwithverylongname.txt", &dentry) != -1) {
		log_msg("read_dentry_by_name very long filename failed");
		return FAIL;
	}
	if(read_dentry_by_name((uint8_t*) "./", &dentry) != -1) {
		log_msg("read_dentry_by_name fancy directory failed");
		return FAIL;
	}

	/* read_dentry_by_index tests */
	if(read_dentry_by_index(0, NULL) != -1) {
		log_msg("read_dentry_by_index null dentry failed");
		return FAIL;
	}
	if(read_dentry_by_index(fs_boot_blk->num_dentries, &dentry) != -1) {
		log_msg("read_dentry_by_index out of bounds failed");
		return FAIL;
	}
	if(read_dentry_by_index(0xFFFFFFFF, &dentry) != -1) {
		log_msg("read_dentry_by_index max index failed");
		return FAIL;
	}

	/* read_data tests*/
	if(read_dentry_by_name((uint8_t*) "frame0.txt", &dentry) != 0) {
		log_msg("read_dentry_by_name frame0.txt failed");
		return FAIL;
	}
	uint32_t inode = dentry.inode;
	uint8_t buf[FILE_CHUNK_SIZE];
	if(read_data(inode, 0, NULL, FILE_CHUNK_SIZE) != -1) {
		log_msg("read_data null buf failed");
		return FAIL;
	}
	if(read_data(fs_boot_blk->num_inode, 0, buf, FILE_CHUNK_SIZE) != -1) {
		log_msg("read_data out of bounds inode failed");
		return FAIL;
	}

	/* file_open tests */
	fd_info_t fd_info;
	if(file_fd_driver.open(NULL, (uint8_t*) "frame0.txt") != -1) {
		log_msg("file_open null fd_info failed");
		return FAIL;
	}
	if(file_fd_driver.open(&fd_info, NULL) != -1) {
		log_msg("file_open null filename failed");
		return FAIL;
	}
	if(file_fd_driver.open(&fd_info, (uint8_t*) "lol") != -1) {
		log_msg("file_open missing file failed");
		return FAIL;
	}
	if(file_fd_driver.open(&fd_info, (uint8_t*) "verylargetextwithverylongname.txt") != -1) {
		log_msg("file_open long filename failed");
		return FAIL;
	}
	if(file_fd_driver.open(&fd_info, (uint8_t*) "") != -1) {
		log_msg("file_open empty filename failed");
		return FAIL;
	}

	/* file_close tests */
	if(file_fd_driver.close(NULL) != -1) {
		log_msg("file_close null fd_info failed");
		return FAIL;
	}

	/* file_read tests */
	if(file_fd_driver.read(NULL, buf, FILE_CHUNK_SIZE) != -1) {
		log_msg("file_read null fd_info failed");
		return FAIL;
	}
	if(file_fd_driver.read(&fd_info, NULL, FILE_CHUNK_SIZE) != -1) {
		log_msg("file_read null buf failed");
		return FAIL;
	}
	if(file_fd_driver.read(&fd_info, buf, -1) != -1) {
		log_msg("file_read negative nbytes failed");
		return FAIL;
	}

	/* file_write tests */
	if(file_fd_driver.write(&fd_info, buf, FILE_CHUNK_SIZE) != -1) {
		log_msg("file_write (did not) failed");
		return FAIL;
	}

	/* directory_open tests */
	if(directory_fd_driver.open(NULL, (uint8_t*) ".") != -1) {
		log_msg("directory_open null fd_info failed");
		return FAIL;
	}
	if(directory_fd_driver.open(&fd_info, NULL) != -1) {
		log_msg("directory_open null filename failed");
		return FAIL;
	}

	/* directory_close tests */
	if(directory_fd_driver.close(NULL) != -1) {
		log_msg("directory_close null fd_info failed");
		return FAIL;
	}

	/* file_read tests */
	if(directory_fd_driver.read(NULL, buf, FILE_CHUNK_SIZE) != -1) {
		log_msg("directory_read null fd_info failed");
		return FAIL;
	}
	if(directory_fd_driver.read(&fd_info, NULL, FILE_CHUNK_SIZE) != -1) {
		log_msg("directory_read null buf failed");
		return FAIL;
	}
	if(directory_fd_driver.read(&fd_info, buf, -1) != -1) {
		log_msg("directory_read negative nbytes failed");
		return FAIL;
	}

	/* file_write tests */
	if(directory_fd_driver.write(&fd_info, buf, FILE_CHUNK_SIZE) != -1) {
		log_msg("directory_write (did not) failed");
		return FAIL;
	}
	return PASS; /* TODO */
}

/* fs_fail_conditions_panic
 * Tests various fail conditions of fs_init
 * Inputs: none
 * Outputs: none
 * Return value: PASS/FAIL
 * Side effects: panics
 * Coverage: fs_init
 * Files: fs.c/h, fd.h */
int fs_fail_conditions_panic() {
	TEST_HEADER;
	fs_boot_blk_t boot_blk;
	/* check null pointer panics */
	fs_init(NULL, (uint8_t*) &boot_blk);
	// fs_init((uint8_t*) &boot_blk, NULL);
	boot_blk.num_data_blk = boot_blk.num_inode = 32;
	/* check missing boot block panic */
	// fs_init((uint8_t*) &boot_blk, (uint8_t*) &boot_blk);
	/* check filesystem too short panic */
	// fs_init((uint8_t*) &boot_blk, (uint8_t*) ((&boot_blk) + 1));
	return FAIL;
}

/* Checkpoint 3 tests */

/* process_switching_test
 * Tests switching back and forth between a few processes
 * Inputs: none
 * Outputs: none
 * Return value: never returns
 * Side effects: never returns, overrides initial kernel stack
 * Coverage: alloc_process jump_to_process switch_to_process
 * Files: process.c/h, swtch.c/h, swtch_asm.S */
int process_switching_test() {
	TEST_HEADER;
	enable_process_switching_test = 1;

	cli(); // Interrupts must be disabled for alloc_process
    /* Execute the first program ("shell") ... */
    pcb_t *shell0 = alloc_process(NULL, (uint8_t*) "shell", 0);
    pcb_t *shell1 = alloc_process(NULL, (uint8_t*) "shell", 0);
    (void) shell0;
    jump_to_process(shell1);
    panic_msg("should never run, FAIL");

	return FAIL;
}

/* variables for keeping track of whether a syscall handler was called */
static volatile int test_syscall_cp3_var;

/* test_syscall_cp3_aux
 * A simple test syscall handler of type syscall_t, used in test_syscall_cp3. */
static int32_t test_syscall_cp3_aux(int32_t arg1, int32_t arg2, int32_t arg3) {
	log_msg("syscall fired! arg1: %x arg2: %x arg3: %x", arg1, arg2, arg3);
	test_syscall_cp3_var = 1;
	return 5;
}

/* fire_syscall
 * Fires the given syscall number, storing the return value back in the same
 * variable. sysnum_retval must be an lvalue (a variable), while the rest
 * can be number literals, or other rvalues.
 * Inputs: sysnum_retval - Gets mapped to EAX, and so this stores the syscall
 *                         number before the interrupt, and the return value
 *                         after the interrupt.
 *         arg1-3 - Arguments one through three, mapped to EBX, ECX, and EDX
 * Outputs: sysnum_retval - See above.
 * Side effects: Fires a syscall interrupt */
#define fire_syscall(sysnum_retval, arg1, arg2, arg3) 						\
		asm volatile("int $0x80" :											\
				"+a"(sysnum_retval) : "b"(arg1), "c"(arg2), "d"(arg3) :	\
				"memory", "cc")

/* test_syscall_cp3
 * Tests the syscall dispatcher by firing a syscall interrupt with various
 * syscall numbers.
 * Inputs: none
 * Outputs: none
 * Return value: PASS/FAIL
 * Side effects: triggers syscall interrupts, temporarily modifies syscall_tbl
 * Coverage: syscall_int syscall_handler
 * Files: idt.c/h idtasm.S syscall.c/h */
int test_syscall_cp3() {
	TEST_HEADER;
	syscall_t *old_tbl[NUM_SYSCALLS];

	memcpy(old_tbl, syscall_tbl, sizeof(syscall_tbl));
	memset(syscall_tbl, 0, sizeof(syscall_tbl));
	syscall_tbl[0] = &test_syscall_cp3_aux;

	int32_t sysnum_retval;

	test_syscall_cp3_var = 0;
	sysnum_retval = 0;
	fire_syscall(sysnum_retval, 0x123, 0x456, 0x789);
	if(sysnum_retval != -1 || test_syscall_cp3_var != 0) {
		log_msg("failed here");
		memcpy(syscall_tbl, old_tbl, sizeof(syscall_tbl));
		return FAIL;
	}
	test_syscall_cp3_var = 0;
	sysnum_retval = 1;
	fire_syscall(sysnum_retval, 0x123, 0x456, 0x789);
	if(sysnum_retval != 5 || test_syscall_cp3_var != 1) {
		log_msg("failed here");
		memcpy(syscall_tbl, old_tbl, sizeof(syscall_tbl));
		return FAIL;
	}
	test_syscall_cp3_var = 0;
	sysnum_retval = 2;
	fire_syscall(sysnum_retval, 0x123, 0x456, 0x789);
	if(sysnum_retval != -1 || test_syscall_cp3_var != 0) {
		log_msg("failed here");
		memcpy(syscall_tbl, old_tbl, sizeof(syscall_tbl));
		return FAIL;
	}

	/* copy saved table back */
	memcpy(syscall_tbl, old_tbl, sizeof(syscall_tbl));
	return PASS;
}

/* Checkpoint 4 tests */
/* Checkpoint 5 tests */

/* pit_test
 * Tests that the PIT IRQ fires at the right rate by printing to the screen.
 * Inputs: none
 * Outputs: none
 * Return value: never returns
 * Side effects: never returns
 * Coverage: pit_init, pit_handler
 * Files: pit.c/h */
int pit_test() {
	TEST_HEADER;
	enable_pit_test = 1;

	int i;
	for(i = 0; i < 100; ++i) {
		asm volatile("hlt");
	}

	enable_pit_test = 0;

	return PASS;
}

/* Test suite entry point */
/* void launch_tests()
 * The starting point for all test calls, devs can selectively enable tests here
 * Inputs / Outputs / Return value: none
 * Side effects: Depends on the tests ran; for non-interfering tests, just printing
 *               a line or two to the screen. For interfering tests, scrambling
 *               the screen, printing lots to it, faulting, etc.
 */
void launch_tests(){
	/* Checkpoint 1 tests */
	// TEST_OUTPUT("idt_test", idt_test());
	// launch your tests here
	/* these tests just print their results without interfering with each other;
	 * they can all be run together */
	// TEST_OUTPUT("paging_no_fault_test", paging_no_fault_test());
	// TEST_OUTPUT("full_idt_test", full_idt_test());
	// TEST_OUTPUT("page_dir_test", page_dir_test());
	// TEST_OUTPUT("test_syscall_cp1", test_syscall_cp1());
	// TEST_OUTPUT("cr2_rw_test", cr2_rw_test());

	/* these tests will cause a fault, or otherwise obscure other
	 * test results; only enable one at a time */
	/* NOTE: some of these have subtests! go to the original functions to uncomment each */
	// TEST_OUTPUT("scrolling_test", scrolling_test());
	/* paging_fault_test has subtests! */
	// TEST_OUTPUT("paging_fault_test", paging_fault_test());
	// TEST_OUTPUT("rtc_main_test", rtc_main_test());
	/* test_ints has subtests! */
	// TEST_OUTPUT("test_ints", test_ints());
	// TEST_OUTPUT("test_div_by_zero", test_div_by_zero());
	// TEST_OUTPUT("test_undef_opcode", test_undef_opcode());
	/* test_idt_oob has subtests! */
	// TEST_OUTPUT("test_idt_oob", test_idt_oob());
	// TEST_OUTPUT("test_irq_reg_oob", test_irq_reg_oob());
	// TEST_OUTPUT("test_irq_null_node", test_irq_null_node());
	// TEST_OUTPUT("test_irq_null_handler", test_irq_null_handler());
	// TEST_OUTPUT("test_irq_double_reg", test_irq_double_reg());
	// TEST_OUTPUT("test_irq_reg_in_use", test_irq_reg_in_use());
	// TEST_OUTPUT("test_irq_no_handlers", test_irq_no_handlers());
	// TEST_OUTPUT("test_irq_unhandled", test_irq_unhandled());


	/* Checkpoint 2 tests */
	/* these tests just print their results without interfering with each other;
     * they can all be run together */
	// TEST_OUTPUT("terminal_read_test", terminal_read_test());
	// TEST_OUTPUT("terminal_write_test", terminal_write_test());
	// TEST_OUTPUT("terminal_open_test", terminal_open_test());
	// TEST_OUTPUT("terminal_close_test", terminal_close_test());
	// TEST_OUTPUT("rtc_openclose_test", rtc_openclose_test());
	// TEST_OUTPUT("rtc_write_test", rtc_write_test());
	// TEST_OUTPUT("rtc_read_test", rtc_read_test());
	// TEST_OUTPUT("rtc_open_rate_test", rtc_open_rate_test());
	// TEST_OUTPUT("fs_fail_conditions", fs_fail_conditions());

    /* these tests will cause a fault, or otherwise obscure other
     * test results; only enable one at a time */
	// TEST_OUTPUT("term_read_write_test", term_read_write_test());
	// TEST_OUTPUT("fs_dir_fd_test", fs_dir_fd_test());
	// TEST_OUTPUT("fs_dentry_index_test", fs_dentry_index_test());
	// TEST_OUTPUT("rtc_freq_test", rtc_freq_test());
	// TEST_OUTPUT("fs_file_fd_test", fs_file_fd_test());
	// TEST_OUTPUT("fs_fail_conditions_panic", fs_fail_conditions_panic());

	/* Checkpoint 3 tests */
	/* these tests just print their results without interfering with each other;
     * they can all be run together */
	// TEST_OUTPUT("test_syscall_cp3", test_syscall_cp3());

	/* these tests will cause a fault, or otherwise obscure other
     * test results; only enable one at a time */
	// TEST_OUTPUT("process_switching_test", process_switching_test());

	/* Checkpoint 4 tests */
	/* Checkpoint 5 tests */
	/* these tests just print their results without interfering with each other;
     * they can all be run together */
	// TEST_OUTPUT("pit_test", pit_test());

	/* these tests will cause a fault, or otherwise obscure other
     * test results; only enable one at a time */
}
