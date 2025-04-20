/* xenia_vga.S - you like breaking userspace don't you
 * image sourced from https://knowyourmeme.com/photos/2609317-xenia-linuxfox */

    .align 4
.global xenia_vga
xenia_vga:
    // the following .raw file was made using the png_to_vga_bytes.py script in this repo
    .incbin "xenia_vga.raw"
