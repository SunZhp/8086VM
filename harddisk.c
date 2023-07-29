#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "mem.h"
#include "harddisk.h"
#include "util/util_file.h"

struct ide g_ide[2] = {0};
struct ide_register g_ide_register = {
	.device = 0,
	.error = 0,
	.sector_count = 0,
	.lba_low = 0,
	.lba_middle = 0,
	.lba_high = 0,
	.device = 0xa0,
	.status = 0,
	.command = 0,
};


static void harddisk_store_lba_l(uint16_t port, uint8_t lba); //lba 0-7
static void harddisk_store_lba_m(uint16_t port, uint8_t lba); //lba 8-15
static void harddisk_store_lba_h(uint16_t port, uint8_t lba); //lba 16-23
static void harddisk_store_lba_e(uint16_t port, uint8_t lba); //lba 24-27
static void harddisk_sector_count(uint16_t port, uint8_t count);	//设置count
static void harddisk_command(uint16_t port, uint8_t command); //设置command
static void harddisk_status(void); 	//获取状态
static void harddisk_read_8(void); 	//一次读取1个字节
static void harddisk_read_16(void); //一次读取2个字节


static struct hdisk* g_hdisk = NULL;
static void _init_harddisk(void){
	g_hdisk = (struct  hdisk*)malloc(sizeof(struct hdisk));
	assert(g_hdisk != NULL);

	g_hdisk->buffer = (uint8_t*)malloc(HDISK_H * HDISK_S * HDISK_M * VM_HDISK_SECTOR);
	assert(g_hdisk->buffer != NULL);
	g_hdisk->data  = 0;
	g_hdisk->pos   = 0;
}

/*
 * 用于同步线程间对于g_ide_register的读写
 */
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;

//获取lba地址
static uint32_t hd_lba(void);
//获取主盘或者从盘信息
static struct hdisk * hd_select(void);

/*
 * 用于执行命令操作
 */
void* harddisk_task_command(void* arg){
	struct vm_file_handle * handle = vm_file_handle_create();

	assert(handle != NULL);

	while(1){
		uint8_t command = g_ide_register.command;

		pthread_mutex_lock(&g_mutex);
		pthread_cond_wait(&g_cond, &g_mutex);

		g_ide_register.status = 0x80;

		//获取硬盘信息
		struct hdisk *hd = hd_select();
		uint32_t lba = 0;

		switch(command){
		case 0xec:	//硬盘识别, 待完善
			break;
		case 0x20: 	//读扇区
			if(g_ide_register.sector_count == 0){
				break;
			}

			lba = hd_lba();
			if(hd->buffer){
				free(hd->buffer);
				hd->buffer = NULL;
			}

			if(hd->buffer == NULL){
				hd->data = (uint64_t)g_ide_register.sector_count * VM_HDISK_SECTOR;
				hd->pos  = 0;
				hd->buffer = (uint8_t*)malloc(hd->data);
				assert(hd->pos == NULL);

				g_ide_register.status = 0x05;
			}

			//读取文件
			vm_file_handle_seek(handle, (uint64_t)lba * VM_HDISK_SECTOR);
			vm_file_handle_read(handle, hd->buffer, hd->data);

			g_ide_register.status = 0x08;

			break;
		case 0x30:	//写扇区  待完善
			break;
		default:
			fprintf(stderr, "do not support hd command 0x%02x\n", command);
			break;
		}

		pthread_mutex_unlock(&g_mutex);
	}
}

void harddisk_init(void){
	pthread_t tid = 0;

	_init_harddisk();

	pthread_create(&tid, NULL, &harddisk_task_command, NULL);

	//只注册了primary通道的硬盘,目前只支持1块硬盘
	pci_register_out_8(0x01f2, harddisk_sector_count);
	pci_register_out_8(0x01f3, harddisk_store_lba_l);
	pci_register_out_8(0x01f4, harddisk_store_lba_m);
	pci_register_out_8(0x01f5, harddisk_store_lba_h);
	pci_register_out_8(0x01f6, harddisk_store_lba_e);
	pci_register_out_8(0x01f7, harddisk_command);

	pci_register_in_8(0x01f7, harddisk_status);
	pci_register_in_8(0x01f0, harddisk_read_8); //读取命令执行结果
	pci_register_in_16(0x01f0, harddisk_read_16);
}

void harddisk_store_lba_l(uint16_t port, uint8_t lba){
	pthread_mutex_lock(&g_mutex);  //防止命令执行过程中产生lba地址的改变

	g_ide_register.lba_low = lba;

	pthread_mutex_unlock(&g_mutex);
}

void harddisk_store_lba_m(uint16_t port, uint8_t lba){
	pthread_mutex_lock(&g_mutex);

	g_ide_register.lba_middle = lba;

	pthread_mutex_unlock(&g_mutex);
}

void harddisk_store_lba_h(uint16_t port, uint8_t lba){
	pthread_mutex_lock(&g_mutex);

	g_ide_register.lba_high = lba;

	pthread_mutex_unlock(&g_mutex);
}

void harddisk_store_lba_e(uint16_t port, uint8_t lba){
	pthread_mutex_lock(&g_mutex);

	g_ide_register.device &= 0x0f;
   	g_ide_register.device |= (lba & 0x0f);

	pthread_mutex_unlock(&g_mutex);
}

void harddisk_sector_count(uint16_t port, uint8_t count){
	pthread_mutex_lock(&g_mutex); //防止命令执行过程中发生count变化

	g_ide_register.sector_count = count;

	pthread_mutex_unlock(&g_mutex);
}

void harddisk_command(uint16_t port, uint8_t command){
	pthread_mutex_lock(&g_mutex); //防止命令执行过程中发生寄存器变化

	g_ide_register.command = command;

	pthread_mutex_unlock(&g_mutex);

	pthread_cond_signal(&g_cond); //通知有新命令进来
}

static void harddisk_status(void){
	pci_setvalue_8(g_ide_register.status);
}

static void harddisk_read_8(void){
	pthread_mutex_lock(&g_mutex);

	struct hdisk *hd = hd_select();

	uint8_t v = 0;

	if(hd->pos < hd->data){
		v = hd->buffer[hd->pos++];
	}
	pci_setvalue_8(v);

	pthread_mutex_unlock(&g_mutex);
}

static void harddisk_read_16(void){
	pthread_mutex_lock(&g_mutex);

	struct hdisk *hd = hd_select();

	uint16_t v = 0;

	if(hd->pos < hd->data - 1){
		v = *(uint16_t*)(&hd->buffer[hd->pos]);
	}
	hd->pos+=2;
	pci_setvalue_16(v);

	pthread_mutex_unlock(&g_mutex);
}

void bios_harddisk_reset(void){
	;
}

uint8_t bios_harddisk_status(void){
	return g_ide_register.status;
}

void bios_harddisk_readsector(addr_t addr, uint32_t lba, uint32_t sector){
	uint32_t bytes = sector * VM_HDISK_SECTOR;

	if(bytes == 0) return;

	uint8_t *buffer = (uint8_t*)malloc(bytes);
	assert(buffer != NULL);

	struct vm_file_handle * handle = vm_file_handle_create();
	assert(handle != NULL);

	vm_file_handle_seek(handle, (uint64_t)lba * VM_HDISK_SECTOR);
	vm_file_handle_read(handle, buffer, bytes);

	vm_write(addr, buffer, bytes);

	free(buffer);
	vm_file_handle_destroy(handle);
}

void bios_harddisk_writesector(addr_t addr, uint32_t lba, uint32_t sector){
	uint32_t bytes = sector * VM_HDISK_SECTOR;

	if(bytes == 0) return;

	uint8_t *buffer = (uint8_t*)malloc(bytes);
	assert(buffer != NULL);

	struct vm_file_handle * handle = vm_file_handle_create();
	assert(handle != NULL);

	vm_file_handle_seek(handle, (uint64_t)lba * VM_HDISK_SECTOR);

	vm_read(addr, buffer, bytes);

	vm_file_handle_write(handle, buffer, bytes);

	free(buffer);
	vm_file_handle_destroy(handle);
}


static struct hdisk * hd_select(void){
	return g_hdisk;
}

static uint32_t hd_lba(void){
	uint32_t lba;

	struct  hdisk* hd = hd_select();
	assert(hd != NULL);

	return hd->pos / VM_HDISK_SECTOR;
}
