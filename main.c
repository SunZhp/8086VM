#include <stdio.h>
#include <assert.h>
#include "config.h" //包含基本的宏定义，需要放在头文件的开始处
#include "cpu.h"
#include "vgui.h"
#include "mem.h"
#include "keyboard.h"
#include "util/util_file.h"
//#include "interupt.h"

extern char *optarg;

int loadhd(void);

static void init_resource(void){
	vm_fprintf(stdout,"init cpu ...\n");
	assert(cpu_init() != 0); 		//cpu初始化
	vm_fprintf(stdout,"init cpu done\n");

	vm_fprintf(stdout,"init virtual memory ...\n");
	assert(mem_init() != 0); 		//内存初始化
	vm_fprintf(stdout,"init virtual memory done\n");

	vm_fprintf(stdout, "init virtual grouph IO interface ...\n");
	assert(vgui_init() != 0); 		//vgio初始化
	vm_fprintf(stdout, "init virtual grouph IO interface done\n");

	//vm_fprintf(stdout, "init interupt ...\n");
	//assert(interupt_init() != 0);	//中断处理器初始化
	//vm_fprintf(stdout, "init interupt done\n");

	vm_fprintf(stdout, "init pci device ...\n");
	assert(pci_init() != 0);	//keyboard初始化
	vm_fprintf(stdout, "init pcievice done\n");

	vm_fprintf(stdout, "init keyboard device ...\n");
	assert(keyboard_init() != 0);	//keyboard初始化
	vm_fprintf(stdout, "init keyboard device done\n");

	vm_fprintf(stdout, "init harddisk ...\n");
	assert(harddisk_init() != 0);	//harddisk初始化
	vm_fprintf(stdout, "init harddisk done\n");
}

static void print_usage(char* progname){
	vm_fprintf(stdout, "%s hardiskpath\n", progname);
}

int main(int argc, char* argv[]){
	if(argc != 2){
		print_usage(argv[0]);
		exit(-1);
	}

	init_resource();

	g_config.hdpath = argv[1];

	//加载磁盘内容
	if(loadhd() < 0){
		vm_fprintf(stderr, "load harddisk %s failed\n", g_config.hdpath);
		return -1;
	}

	//begin execution
	if(cpu_proc() < 0){
		vm_fprintf(stderr, "can not init cpu_proc task\n");
		return -1;
	}

	return 0;
}

int loadhd(void){
	struct vm_file_handle* handle = vm_file_handle_create();
	if(handle == NULL){
		vm_fprintf(stderr, "bad harddisk name\n");
		return -1;
	}

	//uint32_t hdsize = mem_size();
	void* memaddr = mem_mbr();

	int readbytes = vm_file_handle_read(handle, memaddr, 1024*1024);
	if(readbytes <= 0){
		vm_fprintf(stderr, "read harddisk failed\n");
		perror("read haeddisk failed, ");
		vm_file_handle_destroy(handle);
		return -1;
	}

	vm_file_handle_destroy(handle);
	return 0;
}
