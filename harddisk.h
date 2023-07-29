#ifndef VM_HARDDISK_H
#define VM_HARDDISK_H

#define HDISK_IDE 1 //目前只支持IDE类型的磁盘

#define VM_HDISK_SECTOR 512 //固定扇区大小为512

#define HDISK_H   4   //磁头数
#define HDISK_S   64  //每磁道扇区数
#define HDISK_M   480 //柱面数

#include <stdint.h>

/*
 * 表示单块硬盘
 */
struct hdisk{ 
	uint8_t *buffer;    //读取的数据缓冲区
	uint64_t data;		//当前数据的长度
	uint64_t pos;		//表示当前读取sector的位置
};

#define VM_HDISK_IDE_PRIMARY   0
#define VM_HDISK_IDE_SECONDARY 1

/*
 * ide通道
 */
struct ide{
	struct hdisk hd[2]; //hd[0]为主盘 hd[1]为从盘
};

//一般存在2个ide通道
extern struct ide g_ide[2];

struct ide_register{
	uint8_t device; //device寄存器
	uint8_t error;	//读时是error寄存器，写时是feature寄存器
	uint8_t sector_count; //sector count寄存器,用于指定待读写扇区的数量
	uint8_t lba_low;	//lba地址0-7
	uint8_t lba_middle; //lba地址8-15
	uint8_t lba_high;	//lba地址16-23
	uint8_t dev;		//低4位存储lba地址的24-27位
						//第4位主(0)从(1)盘,
						//第5,7位固定式1,
						//第6位LBA开关(LBA->1, CHS->0)
	uint8_t status; //状态寄存器, 只关心0,3,6,7位
					//0位为err位,值为1表示出错,具体原因见error寄存器
					//3位为data request位, 1表示数据已备好
					//6位为DRDY
					//7位为BSY, 1表示磁盘繁忙
	uint8_t command; //command寄存器
};

extern struct ide_register g_ide_register;

void harddisk_init(void);

//提供给bios 0x13中断的接口函数
void bios_harddisk_reset(void); 	//重置磁盘
uint8_t bios_harddisk_status(void); //获取磁盘状态
void bios_harddisk_readsector(addr_t addr, uint32_t lba, uint32_t sector); //读取sector个扇区数据到addr地址空间处
void bios_harddisk_writesector(addr_t addr, uint32_t lba, uint32_t sector); //写入sector个扇区数据到addr地址空间处

#endif
