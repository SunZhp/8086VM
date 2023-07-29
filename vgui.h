#ifndef VM_VGUI_H
#define VM_VGUI_H

#include <stdint.h>
#include "vgio.h"
#include "mem.h"

//打印彩色信息
void print_color(addr_t addr, uint32_t size);
//打印黑白信息
void print_bw(addr_t addr, uint32_t size);
//打印文本信息
void print_text(addr_t addr, uint32_t size);

int vgui_init(void);
int vgui_deinit(void);

//设置光标位置
void vgui_cursor_set(uint8_t x, uint8_t y);
//获取光标位置
void vgui_cursor_get(uint8_t* x, uint8_t* y);
//设置背景色 40-47
void vgui_cursor_bkcolor(int color);
//设置前景色 30-37
void vgui_cursor_fgcolor(int color);
//获取当前位置的字符
int8_t vgui_char(void);
//输出一个字符
void vgui_set_char(uint8_t c);

#endif
