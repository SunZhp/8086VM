#ifndef VM_CONFIG_H
#define VM_CONFIG_H

#define CPU_8086		//cpu 8086
#define INTR_8259A 		//中断控制器 8259A
#include <stdint.h>
#include <stdio.h>

#if !(defined CPU_8086)
	#error "Must specify a cpu platform!"
#endif

#if !(defined INTR_8259A)
	#error "Must specify a interupt controller!"
#endif

struct {
	char * hdpath;
} g_config;

#if 0
void vm_fprintf(FILE* fp, char* fmt, ...);
#else
#define vm_fprintf fprintf
#endif

#endif
