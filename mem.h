#ifndef VM_MEM_H
#define VM_MEM_H

#include <stdint.h>

#ifdef CPU_8086
	#include "8086/cpu.h"
	#include "8086/mem.h"
#else
	#error "CPU Platform can not be NULL"
#endif

int mem_init(void);
uint32_t mem_size(void);
void* mem_addr(void);

void* mem_mbr(void);

#endif
