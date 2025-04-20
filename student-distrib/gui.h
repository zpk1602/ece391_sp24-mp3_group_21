/* gui.h - basic graphical user interface definitions */

#ifndef _GUI_H
#define _GUI_H

#ifndef ASM

extern int osk_enable;
extern int cursor_enable;

void init_gui(void);
void do_render(void);
void display_xenia(void);

#endif // ASM
#endif // _GUI_H
