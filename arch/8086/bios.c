#include <stdio.h>
#include <assert.h>

#include "8086/bios.h"
#include "8086/cpu.h"
#include "8086/mem.h"
#include "vgui.h"
#include "harddisk.h"
#include "keyboard.h"

static void push_stack_reg(cpu8086_core_t* core, uint16_t reg);
static void pop_stack_reg(cpu8086_core_t* core, uint16_t* reg);

typedef void (*bios_ivt_func)(cpu8086_core_t*);

void bios_ivt_videoservice(cpu8086_core_t*);
void bios_ivt_directdiskservice(cpu8086_core_t*);
void bios_ivt_serialportservice(cpu8086_core_t*);
void bios_ivt_miscellaneoussystemservice(cpu8086_core_t*);
void bios_ivt_keyboardservice(cpu8086_core_t*);
void bios_ivt_parallelportservice(cpu8086_core_t*);
void bios_ivt_clockservice(cpu8086_core_t*);

//bios中断，仅实现了常见的几种
struct bios_ivt{
	bios_ivt_func func;
} bios_ivt_default[256] = {
	NULL,	//0x00
	NULL,	//0x01
	NULL,	//0x02
	NULL,	//0x03
	NULL,	//0x04
	NULL,	//0x05
	NULL,	//0x06
	NULL,	//0x07
	NULL,	//0x08
	NULL,	//0x09
	NULL,	//0x0a
	NULL,	//0x0b
	NULL,	//0x0c
	NULL,	//0x0d
	NULL,	//0x0e
	NULL,	//0x0f
	bios_ivt_videoservice, //0x10, 显示服务
	NULL,	//0x11
	NULL,	//0x12
	bios_ivt_directdiskservice, //0x13 直接磁盘服务
	bios_ivt_serialportservice, //0x14 串口服务
	bios_ivt_miscellaneoussystemservice, //0x15 杂项系统服务
	bios_ivt_keyboardservice, 		//0x16 键盘服务
	bios_ivt_parallelportservice, 	//0x17 并行口服务
	NULL,							//0x18
	NULL,							//0x19
	bios_ivt_clockservice,			//0x1a 时钟服务
	NULL,
};

void bios_ivt_exec_8086(uint8_t n){
	cpu8086_core_t* core = get_core();

	FLAGS_TF_SET(core, 0);
	FLAGS_IF_SET(core, 0);

	//压栈
	push_stack_reg(core, core->reg.flags);
	push_stack_reg(core, core->reg.cs);
	push_stack_reg(core, core->reg.ip);

	core->reg.cs = (uint16_t)n * 4 + 2;
	core->reg.ip = (uint16_t)n * 4;

	bios_ivt_func f = bios_ivt_default[n].func;
	if(f){
		f(core);
	}

	//出栈,本应该由iret指令负责，但是bios由软件实现，不在其中添加iret
	//并且后续也不支持对中断的修改，所以出栈也不依靠iret
	pop_stack_reg(core, &core->reg.ip);
	pop_stack_reg(core, &core->reg.cs);
	pop_stack_reg(core, &core->reg.flags);

	FLAGS_TF_SET(core, 1);
	FLAGS_IF_SET(core, 1);
}

static void push_stack_reg(cpu8086_core_t* core, uint16_t reg){
	assert(core && reg);

	addr_t addr = (addr_t)core->reg.ss * 16 + (addr_t)core->reg.sp;
	vm_write_word(addr, reg);

	core->reg.sp -= 2;
}

static void pop_stack_reg(cpu8086_core_t* core, uint16_t* reg){
	assert(core && reg);

	addr_t addr = (addr_t)core->reg.ss * 16 + (addr_t)core->reg.sp;
	*reg = vm_read_word(addr);

	core->reg.sp += 2;
}

void bios_ivt_videoservice(cpu8086_core_t* core){
	uint16_t  i;
	uint16_t  dx;
	uint16_t  cx;
	uint8_t al;
	uint8_t ah = (uint8_t)(core->reg.ax >> 8);
	uint8_t npage;
	uint8_t x;
	uint8_t y;

	switch(ah){
	case 0x00:  //设置显示器模式0x0c   未实现
		break;	
	case 0x01:  //设置光标形状   未实现
		break;
	case 0x02:  //设置光标位置
		npage = (uint8_t)(core->reg.bx >> 8); //bh
		x = (uint8_t)(core->reg.dx);      //dl
		y = (uint8_t)(core->reg.dx >> 8); //dh
		
		vgui_cursor_set(x, y);
		break;
	case 0x03:  //读取光标位置
		x = 0, y = 0;
		vgui_cursor_get(&x, &y);

		dx = ((uint16_t)x) << 8 + (uint16_t)y;
		core->reg.dx = dx;
		break;
	case 0x04:  //读取光笔位置   未实现
		break;
	case 0x05:  //设置显示页   未实现
		break;
	case 0x06:  //初始化或滚屏 未实现
		break;
	case 0x07:  //初始化或滚屏  未实现
		break;
	case 0x08:  //读光标处的字符或属性
		al = vgui_char();
		core->reg.ax = (uint16_t)al;

		break;
	case 0x09:  //在光标处按指定属性显示字符
		al = (uint8_t)core->reg.ax;
		cx = core->reg.cx;

		i = 0;
		for(; i < cx; i++){
			vgui_set_char(al);
		}

		break;
	case 0x0a:  //在当前光标处显示字符
		al = (uint8_t)core->reg.ax;
		cx = core->reg.cx;

		i = 0;
		for(; i < cx; i++){
			vgui_set_char(al);
		}

		break;
	case 0x0b:  //设置调色板、背景色或边框 未实现
		break;
	case 0x0c:  //写图形象素 未实现
		break;
	case 0x0d:  //读图形象素 未实现
		break;
	case 0x0e:  //在Teletype模式下显示字符 未实现
		break;
	case 0x0f:  //读取显示器模式 未实现
		break;
	case 0x10:  //颜色 未实现
		break;
	case 0x11:  //字体 未实现
		break;
	case 0x12:  //显示器的配置 未实现
		break;
	case 0x13:  //在Teletype模式下显示字符串 未实现
		break;
	case 0x1a:  //读取/设置显示组合编码 未实现
		break;
	case 0x1b:  //读取功能/状态信息 未实现
		break;
	case 0x1c:  //保存/恢复显示器状态 未实现
		break;
	default:
		break;
	}
}

void bios_ivt_directdiskservice(cpu8086_core_t* core){
	uint8_t ah = (uint8_t)(core->reg.ax >> 8);
	addr_t addr;
	uint32_t lba;
	uint8_t status;
	uint8_t err;
	uint8_t dreg;
	uint8_t drdy;
	uint8_t bsy;
	uint8_t cblock;
	uint8_t c;
	uint8_t s;
	uint8_t d;
	uint8_t h;
	uint32_t H;
	uint32_t S;

	switch(ah){
	case 0x00:  // 磁盘系统复位
		bios_harddisk_reset();
		//设置CF,以及ah
		FLAGS_CF_SET(core, 0);
		core->reg.ax &= 0x00ff;

		break;
	case 0x01:  // 读取磁盘系统状态
	case 0x10:  // 读取驱动器状态
		status = bios_harddisk_status();
		err  = status & 0x01;
		dreg = status & 0x08;
		drdy = status & 0x40;
		bsy  = status & 0x80;

		if(err == 0 && bsy == 0){
			core->reg.ax = 0;
		} else if(dreg == 0){
			core->reg.ax = 0xaa;
		} else if(bsy == 1){
			core->reg.ax = 0xcc;
		} else{
			core->reg.ax = 0x01;
		}

		break;
	case 0x0E:  // 读扇区缓冲区
	case 0x02:  // 读扇区
		cblock = (uint8_t)(core->reg.ax); //扇区数
		c = (uint8_t)(core->reg.cx >> 8); //柱面
		s = (uint8_t)(core->reg.cx); 	  //扇区
		h = (uint8_t)(core->reg.dx >> 8); //磁头
		d = (uint8_t)(core->reg.dx);	  //驱动器, 无用
		
		H = HDISK_H; // 每磁柱磁头数
		S = HDISK_S; // 每磁道扇区数
		//转换成lba地址
		lba = ((uint16_t)c * H + (uint16_t)h ) * S + (uint16_t)s - 1;
		
		addr = vm_addr_calc(core->reg.es, core->reg.bx);

		bios_harddisk_readsector(addr, lba, (uint32_t)cblock);

		FLAGS_CF_SET(core, 0);

		break;
	case 0x03:  // 写扇区
	case 0x0F:  // 写扇区缓冲区
		cblock = (uint8_t)(core->reg.ax); //扇区数
		c = (uint8_t)(core->reg.cx >> 8); //柱面
		s = (uint8_t)(core->reg.cx); 	  //扇区
		h = (uint8_t)(core->reg.dx >> 8); //磁头
		d = (uint8_t)(core->reg.dx);	  //驱动器, 无用
		
		H = HDISK_H; //每磁柱磁头数
		S = HDISK_S; //每磁道扇区数
		//转换成lba地址
		lba = ((uint16_t)c * H + (uint16_t)h ) * S + (uint16_t)s - 1;
		
		addr = vm_addr_calc(core->reg.es, core->reg.bx);

		bios_harddisk_writesector(addr, lba, (uint32_t)cblock);

		FLAGS_CF_SET(core, 0);

		break;
	case 0x04:  // 验扇区 未实现
		break;
	case 0x05:  // 格式化磁道 未实现
		break;
	case 0x06:  // 格式化坏磁道 未实现
		break;
	case 0x07:  // 格式化驱动器 未实现
		break;
	case 0x08:  // 读取驱动器参数 未实现
	case 0x09:  // 初始化硬盘参数 未实现
	case 0x0A:  // 读长扇区 未实现
		break;
	case 0x0B:  // 写长扇区 未实现
		break;
	case 0x0C:  // 查寻 未实现
		break;
	case 0x0D:  // 硬盘系统复位 未实现
		break;
	case 0x11:  // 校准驱动器 未实现
	case 0x12:  // 控制器RAM诊断  未实现
	case 0x13:  // 控制器驱动诊断 未实现
	case 0x14:  // 控制器内部诊断 未实现
	case 0x15:  // 读取磁盘类型 未实现
	case 0x16:  // 读取磁盘变化状态 未实现
	case 0x17:  // 设置磁盘类型 未实现
	case 0x18:  // 设置格式化媒体类型 未实现
	case 0x19:  // 磁头保护 未实现
	case 0x1A:  // 格式化ESDI驱动器 未实现
		break;
	}
}

void bios_ivt_serialportservice(cpu8086_core_t* core){
	return;
}

void bios_ivt_miscellaneoussystemservice(cpu8086_core_t* core){
	return;
}

void bios_ivt_keyboardservice(cpu8086_core_t* core){
	uint8_t ah = (uint8_t)(core->reg.ax >> 8);

	switch(ah){
	case 0:
		core->reg.ax = keyboard_read();
		break;
	}
}

void bios_ivt_parallelportservice(cpu8086_core_t* core){
	return;
}

void bios_ivt_clockservice(cpu8086_core_t* core){
	return;
}
