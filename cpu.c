#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include "cpu.h"
#include "config.h"

static int _cpu_proc(void){
#ifdef CPU_8086
	return cpu8086_proc();
#else
	vm_fprintf(stderr, "cpu platform not supported\n");
	assert(1);
#endif
}

/*
 * cpu指令处理放在一个单独的线程中
 */
void* cpu_proc_thread(void* arg){
	while(1){
		if(_cpu_proc() < 0){
			vm_fprintf(stderr,"cpu process error!\n");
			exit(-1);
		}
	}

	return NULL;
}

int cpu_proc(void){
	pthread_t pid;

	if(pthread_create(&pid, NULL, cpu_proc_thread, NULL)){
		vm_fprintf(stderr, "create cpu_proc task failed\n");
		return -1;
	}

	pthread_join(pid, NULL);

	return 0;
}

/*
 * 初始化cpu 资源， 0 成功， 否则失败
 */
int cpu_init(){
#ifdef CPU_8086
	return cpu8086_init();
#endif

	return 0;
}

