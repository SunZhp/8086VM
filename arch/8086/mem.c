#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "8086/cpu.h"
#include "8086/mem.h"
#include "vgui.h"
#include "config.h"

//内存分布
struct vm_mem{
	//0 - 0x3ff 中断向量表
	uint8_t ivt[1024];
	//0x400 - 0x4ff bios数据区
	uint8_t biosda[256];
	//0x500 - 0x7bff 可用区域 30464Byte
	uint8_t a[30464];
	//0x7c00 - 0x7dff MBR
	uint8_t mbr[512];
	//0x7e00 - 0x9fbff 可用区域 622080Byte
	uint8_t b[622080];
	//0x9fc00 - 0x9ffff EBDA
	uint8_t ebda[1024];
	//0xa0000 - 0xaffff 彩色适配器
	uint8_t c_adapter[65536];
	//0xb0000 - 0xb7fff 黑白适配器
	uint8_t bw_adapter[32768];
	//0xb8000 - 0xbffff 文本模式适配器
	uint8_t t_adapter[32768];
	//0xc0000 - 0xc7fff 显示适配器
	uint8_t adapter[32768];
	//0xc8000 - 0xeffff 映射硬件适配器的ROM或内存映射式I/O
	uint8_t rom[163840];
	//0xf0000 - 0xfffff 系统bios范围
	uint8_t bios[65536];
} __attribute__((packed));

//定义全局的内存结构体
static struct vm_mem * g_vm_mem = NULL;

int vm_init(void){
	g_vm_mem = (struct vm_mem*)calloc(sizeof(struct vm_mem), 1);

	if(g_vm_mem == NULL){
		vm_fprintf(stderr, "vm_init failed!\n");
		return 0;
	}

	return 1;
}

uint8_t vm_read_byte(addr_t maddr){
	uint8_t m_byte = 0;

	if(maddr <= 0x3ff){
		m_byte = g_vm_mem->ivt[maddr];
	} else if(maddr >= 0x400 && maddr <= 0xfff ){
		m_byte = g_vm_mem->biosda[maddr - 0x400];
	} else if(maddr >= 0x500 && maddr <= 0x7bff ){
		m_byte = g_vm_mem->a[maddr - 0x500];
	} else if(maddr >= 0x7c00 && maddr <= 0x7dff ){
		m_byte = g_vm_mem->mbr[maddr - 0x7c00];
	} else if(maddr >= 0x7e00 && maddr <= 0x9fbff ){
		m_byte = g_vm_mem->b[maddr - 0x7e00];
	} else if(maddr >= 0x9fc00 && maddr <= 0x9ffff ){
		m_byte = g_vm_mem->ebda[maddr - 0x9fc00];
	} else if(maddr >= 0xa0000 && maddr <= 0xaffff ){
		m_byte = g_vm_mem->c_adapter[maddr - 0xa0000];
	} else if(maddr >= 0xb0000 && maddr <= 0xb7fff ){
		m_byte = g_vm_mem->bw_adapter[maddr - 0xb0000];
	} else if(maddr >= 0xb8000 && maddr <= 0xbffff ){
		m_byte = g_vm_mem->t_adapter[maddr - 0xb8000];
	} else if(maddr >= 0xc0000 && maddr <= 0xc7fff ){
		m_byte = g_vm_mem->adapter[maddr - 0xc0000];
	} else if(maddr >= 0xc8000 && maddr <= 0xe7fff ){
		m_byte = g_vm_mem->rom[maddr - 0xc8000];
	} else if(maddr >= 0xf0000 && maddr <= 0xfffff ){
		m_byte = g_vm_mem->bios[maddr - 0xf0000];
	} else {
		vm_fprintf(stderr,"memory %u overlap\n",maddr);
		m_byte = 0;
	}

	return m_byte;
}

uint16_t vm_read_word(addr_t maddr){
	uint16_t m_word = 0;

	if(maddr < 0x3ff){
		m_word = *(uint16_t*)(&(g_vm_mem->ivt[maddr]));
	} else if(maddr >= 0x400 && maddr < 0xfff ){
		m_word = *(uint16_t*)(&g_vm_mem->biosda[maddr - 0x400]);
	} else if(maddr >= 0x500 && maddr < 0x7bff ){
		m_word = *(uint16_t*)(&g_vm_mem->a[maddr - 0x500]);
	} else if(maddr >= 0x7c00 && maddr < 0x7dff ){
		m_word = *(uint16_t*)(&g_vm_mem->mbr[maddr - 0x7c00]);
	} else if(maddr >= 0x7e00 && maddr < 0x9fbff ){
		m_word = *(uint16_t*)(&g_vm_mem->b[maddr - 0x7e00]);
	} else if(maddr >= 0x9fc00 && maddr < 0x9ffff){
		m_word = *(uint16_t*)(&g_vm_mem->ebda[maddr - 0x9fc00]);
	} else if(maddr >= 0xa0000 && maddr < 0xaffff ){
		m_word = *(uint16_t*)(&g_vm_mem->c_adapter[maddr - 0xa0000]);
	} else if(maddr >= 0xb0000 && maddr < 0xb7fff ){
		m_word = *(uint16_t*)(&g_vm_mem->bw_adapter[maddr - 0xb0000]);
	} else if(maddr >= 0xb8000 && maddr < 0xbffff ){
		m_word = *(uint16_t*)(&g_vm_mem->t_adapter[maddr - 0xb8000]);
	} else if(maddr >= 0xc0000 && maddr < 0xc7fff ){
		m_word = *(uint16_t*)(&g_vm_mem->adapter[maddr - 0xc0000]);
	} else if(maddr >= 0xc8000 && maddr < 0xe7fff ){
		m_word = *(uint16_t*)(&g_vm_mem->rom[maddr - 0xc8000]);
	} else if(maddr >= 0xf0000 && maddr < 0xfffff ){
		m_word = *(uint16_t*)(&g_vm_mem->bios[maddr - 0xf0000]);
	} else {
		vm_fprintf(stderr,"memory %u overlap\n",maddr);
		m_word = 0;
	}

	return m_word;
}

/*
 * 写内存同时需要执行对应的动作，如bw_adapter需要在展示对应的黑白窗口
 */
int vm_write_byte(addr_t maddr, uint8_t byte){
	if(maddr <= 0x3ff){
		g_vm_mem->ivt[maddr] = byte;
	} else if(maddr >= 0x400 && maddr <= 0xfff ){
		g_vm_mem->biosda[maddr - 0x400] = byte;
	} else if(maddr >= 0x500 && maddr <= 0x7bff ){
		g_vm_mem->a[maddr - 0x500] = byte;
	} else if(maddr >= 0x7c00 && maddr <= 0x7dff ){
		g_vm_mem->mbr[maddr - 0x7c00] = byte;
	} else if(maddr >= 0x7e00 && maddr <= 0x9fbff ){
		g_vm_mem->b[maddr - 0x7e00] = byte;
	} else if(maddr >= 0x9fc00 && maddr <= 0x9ffff ){
		g_vm_mem->ebda[maddr - 0x9fc00] = byte;
	} else if(maddr >= 0xa0000 && maddr <= 0xaffff ){
		//print_color(0xa0000, 0x010000);
		//暂不支持彩色显示适配器

		g_vm_mem->c_adapter[maddr - 0xa0000] = byte;
	} else if(maddr >= 0xb0000 && maddr <= 0xb7fff ){
		//print_bw(0xb0000, 0x8000);
		//暂不支持黑白显示适配器

		g_vm_mem->bw_adapter[maddr - 0xb0000] = byte;
	} else if(maddr >= 0xb8000 && maddr <= 0xbffff ){
		if(maddr % 2 == 0){
			addr_t addr = maddr;
			uint32_t nword = (maddr - 0xb8000) / 2;
			uint32_t x = nword % VGIO_WIDTH;
			uint32_t y = nword / VGIO_WIDTH;

			//vgui_cursor_set(x, y);
			vgui_set_char(byte);
		}

		g_vm_mem->t_adapter[maddr - 0xb8000] = byte;
	} else if(maddr >= 0xc0000 && maddr <= 0xc7fff ){
		g_vm_mem->adapter[maddr - 0xc0000] = byte;
	} else if(maddr >= 0xc8000 && maddr <= 0xe7fff ){
		g_vm_mem->rom[maddr - 0xc8000] = byte;
	} else if(maddr >= 0xf0000 && maddr <= 0xfffff ){
		g_vm_mem->bios[maddr - 0xf0000] = byte;
	} else {
		vm_fprintf(stderr,"memory %u overlap\n",maddr);
		return -1;
	}

	return 0;
}

/*
 * 写内存同时需要执行对应的动作，如bw_adapter需要在展示对应的黑白窗口
 */
int vm_write_word(addr_t maddr, uint16_t word){
	if(maddr < 0x3ff){
		((uint16_t*)g_vm_mem->ivt)[maddr] = word;
	} else if(maddr >= 0x400 && maddr < 0xfff ){
		((uint16_t*)g_vm_mem->biosda)[maddr - 0x400] = word;
	} else if(maddr >= 0x500 && maddr < 0x7bff ){
		((uint16_t*)g_vm_mem->a)[maddr - 0x500] = word;
	} else if(maddr >= 0x7c00 && maddr < 0x7dff ){
		((uint16_t*)g_vm_mem->mbr)[maddr - 0x7c00] = word;
	} else if(maddr >= 0x7e00 && maddr < 0x9fbff ){
		((uint16_t*)g_vm_mem->b)[maddr - 0x7e00] = word;
	} else if(maddr >= 0x9fc00 && maddr < 0x9ffff ){
		((uint16_t*)g_vm_mem->ebda)[maddr - 0x9fc00] = word;
	} else if(maddr >= 0xa0000 && maddr < 0xaffff ){
		((uint16_t*)g_vm_mem->c_adapter)[maddr - 0xa0000] = word;
	} else if(maddr >= 0xb0000 && maddr < 0xb7fff ){
		((uint16_t*)g_vm_mem->bw_adapter)[maddr - 0xb0000] = word;
	} else if(maddr >= 0xb8000 && maddr < 0xbffff ){
		if(maddr % 2 == 0){
			addr_t addr = maddr;
		} else {
			addr_t addr = maddr + 1;
		}

		uint32_t nword = (maddr - 0xb8000) / 2;
		uint32_t x = nword % VGIO_WIDTH;
		uint32_t y = nword / VGIO_WIDTH;

		vgui_cursor_set(x, y);
		vgui_set_char((uint8_t)(word & 0x00ff));

		((uint16_t*)g_vm_mem->t_adapter)[maddr - 0xb8000] = word;
	} else if(maddr >= 0xc0000 && maddr < 0xc7fff ){
		((uint16_t*)&g_vm_mem->adapter)[maddr - 0xc0000] = word;
	} else if(maddr >= 0xc8000 && maddr < 0xe7fff ){
		((uint16_t*)&g_vm_mem->rom)[maddr - 0xc8000] = word;
	} else if(maddr >= 0xf0000 && maddr < 0xfffff ){
		((uint16_t*)&g_vm_mem->bios)[maddr - 0xf0000] = word;
	} else {
		vm_fprintf(stderr,"memory %u overlap\n",maddr);
		return -1;
	}

	return 0;
}

int vm_write(addr_t maddr, uint8_t* content, uint16_t length){
	int i = 0;
	int rcode = 0;
	for(; i < length; i++){
		rcode = vm_write_byte(maddr + i, content[i]);
		if(rcode < 0){
			break;
		}
	}

	return rcode;
}

//读取指令信息
uint8_t instruct_read_byte(){
	cpu8086_core_t * core = get_core();

	addr_t addr = vm_addr_calc(core->reg.cs, core->reg.ip);
	core->reg.ip++;

	return vm_read_byte(addr);
}

uint16_t instruct_read_word(){
	cpu8086_core_t * core = get_core();

	addr_t addr = vm_addr_calc(core->reg.cs, core->reg.ip);
	core->reg.ip += 2;

	return vm_read_word(addr);
}

addr_t vm_addr_calc(uint16_t base, uint16_t offset){
	return (addr_t)base * 16 + (addr_t)offset;
}

uint32_t vm_size(void){
	return sizeof(struct vm_mem);
}

/*
 * 获取虚拟内存的地址
 */
void* vm_addr(void){
	return g_vm_mem;
}

void* vm_mbr(void){
	return g_vm_mem->mbr;
}

int vm_read(addr_t maddr, uint8_t* buffer, uint16_t length){
	int i = 0;
	for(; i < length; i++){
		buffer[i] = vm_read_byte(maddr + i);
	}

	return 0;
}

uint32_t vm_read_dword(addr_t maddr){
	uint32_t m_word = 0;

	if(maddr < 0x3ff){
		m_word = ((uint32_t*)g_vm_mem->ivt)[maddr];
	} else if(maddr >= 0x400 && maddr < 0xfff ){
		m_word = ((uint32_t*)g_vm_mem->biosda)[maddr - 0x400];
	} else if(maddr >= 0x500 && maddr < 0x7bff ){
		m_word = ((uint32_t*)g_vm_mem->a)[maddr - 0x500];
	} else if(maddr >= 0x7c00 && maddr < 0x7dff ){
		m_word = ((uint32_t*)g_vm_mem->mbr)[maddr - 0x7c00];
	} else if(maddr >= 0x7e00 && maddr < 0x9fbff ){
		m_word = ((uint32_t*)g_vm_mem->b)[maddr - 0x7e00];
	} else if(maddr >= 0x9fc00 && maddr < 0x9ffff ){
		m_word = ((uint32_t*)g_vm_mem->ebda)[maddr - 0x9fc00];
	} else if(maddr >= 0xa0000 && maddr < 0xaffff ){
		m_word = ((uint32_t*)g_vm_mem->c_adapter)[maddr - 0xa0000];
	} else if(maddr >= 0xb0000 && maddr < 0xb7fff ){
		m_word = ((uint32_t*)g_vm_mem->bw_adapter)[maddr - 0xb0000];
	} else if(maddr >= 0xb8000 && maddr < 0xbffff ){
		m_word = ((uint32_t*)g_vm_mem->t_adapter)[maddr - 0xb8000];
	} else if(maddr >= 0xc0000 && maddr < 0xc7fff ){
		m_word = ((uint32_t*)&g_vm_mem->adapter)[maddr - 0xc0000];
	} else if(maddr >= 0xc8000 && maddr < 0xe7fff ){
		m_word = ((uint32_t*)&g_vm_mem->rom)[maddr - 0xc8000];
	} else if(maddr >= 0xf0000 && maddr < 0xfffff ){
		m_word = ((uint32_t*)&g_vm_mem->bios)[maddr - 0xf0000];
	} else {
		vm_fprintf(stderr,"memory %u overlap\n",maddr);
		m_word = 0;
	}

	return m_word;
}


