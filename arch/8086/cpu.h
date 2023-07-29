#ifndef VM_CPU_8086_H
#define VM_CPU_8086_H

#include <stdint.h>

typedef struct registers{
	uint16_t ax;
	uint16_t bx;
	uint16_t cx;
	uint16_t dx;
	uint16_t ip;
	uint16_t sp;
	uint16_t bp;
	uint16_t si;
	uint16_t di;
	uint16_t ss;
	uint16_t cs;
	uint16_t ds;
	uint16_t es;
	uint16_t flags;		//每次执行完指令需要更新此寄存器
} __attribute__((packed)) registers_t ;

//预留多cpu接口，目前只有一个，但是后续要扩展成多个
typedef struct cpu8086_core{
	registers_t reg;
	uint32_t	oldip; //指向cpu上一个指令读取位置
	uint32_t 	halt;	//halt标志位
	int16_t	core;	//记录当前cpuid
} cpu8086_core_t;

#define FLAGS_CF(c) (c->reg.flags & 0x0001)
#define FLAGS_PF(c) (c->reg.flags & 0x0004)
#define FLAGS_AF(c) (c->reg.flags & 0x0010)
#define FLAGS_ZF(c) (c->reg.flags & 0x0040)
#define FLAGS_SF(c) (c->reg.flags & 0x0080)
#define FLAGS_TF(c) (c->reg.flags & 0x0100)
#define FLAGS_IF(c) (c->reg.flags & 0x0200)
#define FLAGS_DF(c) (c->reg.flags & 0x0400)
#define FLAGS_OF(c) (c->reg.flags & 0x0800)

#define FLAGS_CF_SET(c,x) (c->reg.flags | ((x & 0x0001)))
#define FLAGS_PF_SET(c,x) (c->reg.flags | ((x & 0x0001 << 2)))
#define FLAGS_AF_SET(c,x) (c->reg.flags | ((x & 0x0001 << 4)))
#define FLAGS_ZF_SET(c,x) (c->reg.flags | ((x & 0x0001 << 6)))
#define FLAGS_SF_SET(c,x) (c->reg.flags | ((x & 0x0001 << 7)))
#define FLAGS_TF_SET(c,x) (c->reg.flags | ((x & 0x0001 << 8)))
#define FLAGS_IF_SET(c,x) (c->reg.flags | ((x & 0x0001 << 9)))
#define FLAGS_DF_SET(c,x) (c->reg.flags | ((x & 0x0001 << 10)))
#define FLAGS_OF_SET(c,x) (c->reg.flags | ((x & 0x0001 << 11)))

/*
 * 处理cpu指令
 * 返回值为处理的字节数,-1为处理失败
 */
int cpu8086_proc(void);
cpu8086_core_t* get_core(void);

int cpu8086_init(void);

#endif
