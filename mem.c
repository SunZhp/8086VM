#include "mem.h"

int mem_init(void){
#ifdef CPU_8086
	return vm_init();
#endif
}

uint32_t mem_size(void){
#ifdef CPU_8086
	return vm_size();
#endif
}

void* mem_addr(void){
#ifdef CPU_8086
	return vm_addr();
#endif
}

void* mem_mbr(void){
#ifdef CPU_8086
	return vm_mbr();
#endif
}
