#ifndef VM_BIOS_8086_H
#define VM_BIOS_8086_H

#include <stdint.h>

/*
 * 执行Bios中断处理函数,n(a中断号)
 */
void bios_ivt_exec_8086(uint8_t n);

#endif
