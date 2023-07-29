#ifndef VM_CPU_H

#define VM_CPU_H

#ifdef CPU_8086
	#include "8086/cpu.h"

	#define cpu_core_t cpu8086_core_t
#else 
	#error CPU platform can not be NULL
#endif

//获取当前的cpu core
extern cpu_core_t * get_core(void);
//初始化cpu资源
int cpu_init(void);
//处理cpu指令
int cpu_proc(void);

#endif

