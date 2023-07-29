#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "8086/cpu.h"
#include "8086/bios.h"
#include "8086/mem.h"
#include "8086/pci.h"
#include "config.h"

#define SWAP(x,y) do{typeof(x) __t = (x); (x) = (y); (y) = __t;}while(0)

enum {
	FLAGS_C,
	FLAGS_P,
	FLAGS_A,
	FLAGS_Z,
	FLAGS_S,
	FLAGS_T,
	FLAGS_I,
	FLAGS_D,
	FLAGS_O,
};

cpu8086_core_t g_cpu8086_core = {{0},0,0,0};

void update_flags(int type, int x){
	cpu8086_core_t * core = get_core();

	switch(x){
		case FLAGS_C:
			FLAGS_CF_SET(core, x); break;
		case FLAGS_P:
			FLAGS_PF_SET(core, x); break;
		case FLAGS_A:
			FLAGS_AF_SET(core, x); break;
		case FLAGS_Z:
			FLAGS_ZF_SET(core, x); break;
		case FLAGS_S:
			FLAGS_SF_SET(core, x); break;
		case FLAGS_T:
			FLAGS_TF_SET(core, x); break;
		case FLAGS_I:
			FLAGS_IF_SET(core, x); break;
		case FLAGS_D:
			FLAGS_DF_SET(core, x); break;
		case FLAGS_O:
			FLAGS_OF_SET(core, x); break;
		default:
			break;
	}
}

cpu8086_core_t * get_core(void){
	return &g_cpu8086_core;
}

/*
 * 操作数
 */
typedef union {
	addr_t 	   addr;	//地址
	uint16_t*  reg;	//寄存器地址
	uint16_t   im;	//立即数
	uint8_t    op;
	uint16_t   offset;	//偏移
	uint16_t   segment;	//段基址
} operand_un;

#define OPERAND_REG8   		0 
#define OPERAND_REG16  		1 
#define OPERAND_ADDR   		2
#define OPERAND_IM8    		3
#define OPERAND_IM16   		4
#define OPERAND_OP	   		5
#define OPERAND_IP8 		6
#define OPERAND_IP16 		7
#define OPERAND_SEGMENT	    8

/*
 * 源目的操作数
 */
struct operand {
	int noperand;			//操作数个数 0，1，2，3
	//当且仅当同一个opcode表示多个操作时为3
	//此时第一个操作数为操作码
	uint8_t operand1_type;
	uint8_t operand2_type;
	uint8_t operand3_type;
	operand_un operand1;
	operand_un operand2;
	operand_un operand3;
	char alias_operand1[32];	//用于打印反汇编信息
	char alias_operand2[32];
	char alias_operand3[32];
};

/*
 * 用于可视化打印当前的指令内容, MOD = 11
 */
struct register_alias_mod11 { //数组下标为寄存器名对应的机器码
	char *name_8;  //8位寄存器名
	char *name_16; //16位寄存器名
} register_alias_mod11[] = { //通用寄存器
	{"al", "ax"},
	{"cl", "cx"},
	{"dl", "dx"},
	{"bl", "bx"},
	{"ah", "sp"},
	{"ch", "bp"},
	{"dh", "si"},
	{"bh", "di"}
};

/*
 * 用于可视化打印当前的指令内容, MOD = 0 MOD = 0 MOD = 10
 */
struct register_alias_mod {
	char *name;
} register_alias_mod00[] = {
	"bx + si",
	"bx + di",
	"bp + si",
	"bp + di",
	"si",
	"di",
	"",
	"bx"
}, register_alias_mod01[] = { //数据长度为8
	"bx + si + %d",
	"bx + di + %d",
	"bp + si + %d",
	"bp + di + %d",
	"si + %d",
	"di + %d",
	"bp + %d",
	"bx + %d"
}, register_alias_mod10[] = { //数据长度为16
	"bx + si + %d",
	"bx + di + %d",
	"bp + si + %d",
	"bp + di + %d",
	"si + %d",
	"di + %d",
	"bp + %d",
	"bx + %d"
};

char* register_alias_sement[] = { //段寄存器
	"es", "cs", "ss", "ds"
};

/*
 * Mod 0sr r/m 16Bit 格式解析
 */
int parse_format_seg2rm_16(struct operand*);

/*
 * Mod reg r/m  8Bit 格式解析
 */
int parse_format_reg2rm_8(struct operand*);

/*
 * Mod reg r/m  8Bit 格式解析, 只用于LEA指令
 */
int parse_format_reg2rm_lea(struct operand*);

/*
 * Mod reg r/m  8Bit 格式解析
 */
int parse_format_reg2rm_16(struct operand*);

/*
 * Data  8Bit 格式解析
 */
int parse_format_imm_8(struct operand*);

/*
 * Data  16Bit 格式解析
 */
int parse_format_imm_16(struct operand*);

/*
 * IP-Inc8  8Bit 格式解析
 */
int parse_format_ipinc_8(struct operand*);

/*
 * IP-Inc16  16Bit 格式解析
 */
int parse_format_ipinc_16(struct operand*);

/*
 * Mod 000-111 r/m Imm 8Bit 格式解析
 */
int parse_format_rm2imm_8(struct operand*);

/*
 * Mod 000-111 r/m Imm 16Bit 格式解析
 */
int parse_format_rm2imm_16(struct operand*);

/*
 * disp seg 格式解析，主要用于Call指令
 */
int parse_format_call_16(struct operand*);

/*
 * Mod 000-111 r/m 8Bit 格式解析
 */
int parse_format_table_rm_8(struct operand*);

/*
 * Mod 000-111 r/m 16Bit 格式解析
 */
int parse_format_table_rm_16(struct operand*);


/*
 * 读取并处理一条指令
 */
static int cpu8086_proc_instruction(void);

/*
 * #################指令处理函数##########################
 */
int instruct_process_add_reg2rm_8(struct operand* oper);
int instruct_process_add_reg2rm_16(struct operand* oper);
int instruct_process_add_rm2reg_8(struct operand* oper);
int instruct_process_add_rm2reg_16(struct operand* oper);
int instruct_process_add_i8al(struct operand* oper);
int instruct_process_add_i16ax(struct operand* oper);
int instruct_process_push_es(struct operand* oper);
int instruct_process_pop_es(struct operand* oper);
int instruct_process_or_reg2rm_8(struct operand* oper);
int instruct_process_or_reg2rm_16(struct operand* oper);
int instruct_process_or_rm2reg_8(struct operand* oper);
int instruct_process_or_rm2reg_16(struct operand* oper);
int instruct_process_or_i8al(struct operand* oper);
int instruct_process_or_i16ax(struct operand* oper);
int instruct_process_push_cs(struct operand* oper);
int instruct_process_adc_reg2rm_8(struct operand* oper);
int instruct_process_adc_reg2rm_16(struct operand* oper);
int instruct_process_adc_rm2reg_8(struct operand* oper);
int instruct_process_adc_rm2reg_16(struct operand* oper);
int instruct_process_adc_i8al(struct operand* oper);
int instruct_process_adc_i16ax(struct operand* oper);
int instruct_process_push_ss(struct operand* oper);
int instruct_process_pop_ss(struct operand* oper);
int instruct_process_sbb_reg2rm_8(struct operand* oper);
int instruct_process_sbb_reg2rm_16(struct operand* oper);
int instruct_process_sbb_rm2reg_8(struct operand* oper);
int instruct_process_sbb_rm2reg_16(struct operand* oper);
int instruct_process_sbb_i8al(struct operand* oper);
int instruct_process_sbb_i16ax(struct operand* oper);
int instruct_process_push_ds(struct operand* oper);
int instruct_process_pop_ds(struct operand* oper);
int instruct_process_and_reg2rm_8(struct operand* oper);
int instruct_process_and_reg2rm_16(struct operand* oper);
int instruct_process_and_rm2reg_8(struct operand* oper);
int instruct_process_and_rm2reg_16(struct operand* oper);
int instruct_process_and_i8al(struct operand* oper);
int instruct_process_and_i16ax(struct operand* oper);
int instruct_process_daa(struct operand* oper);
int instruct_process_sub_reg2rm_8(struct operand* oper);
int instruct_process_sub_reg2rm_16(struct operand* oper);
int instruct_process_sub_rm2reg_8(struct operand* oper);
int instruct_process_sub_rm2reg_16(struct operand* oper);
int instruct_process_sub_i8al(struct operand* oper);
int instruct_process_sub_i16ax(struct operand* oper);
int instruct_process_das(struct operand* oper);
int instruct_process_xor_reg2rm_8(struct operand* oper);
int instruct_process_xor_reg2rm_16(struct operand* oper);
int instruct_process_xor_rm2reg_8(struct operand* oper);
int instruct_process_xor_rm2reg_16(struct operand* oper);
int instruct_process_xor_i8al(struct operand* oper);
int instruct_process_xor_i16ax(struct operand* oper);
int instruct_process_aaa(struct operand* oper);
int instruct_process_cmp_reg2rm_8(struct operand* oper);
int instruct_process_cmp_reg2rm_16(struct operand* oper);
int instruct_process_cmp_rm2reg_8(struct operand* oper);
int instruct_process_cmp_rm2reg_16(struct operand* oper);
int instruct_process_cmp_i8al(struct operand* oper);
int instruct_process_cmp_i16ax(struct operand* oper);
int instruct_process_aas(struct operand* oper);
int instruct_process_inc_ax(struct operand* oper);
int instruct_process_inc_cx(struct operand* oper);
int instruct_process_inc_dx(struct operand* oper);
int instruct_process_inc_bx(struct operand* oper);
int instruct_process_inc_sp(struct operand* oper);
int instruct_process_inc_bp(struct operand* oper);
int instruct_process_inc_si(struct operand* oper);
int instruct_process_inc_di(struct operand* oper);
int instruct_process_dec_ax(struct operand* oper);
int instruct_process_dec_cx(struct operand* oper);
int instruct_process_dec_dx(struct operand* oper);
int instruct_process_dec_bx(struct operand* oper);
int instruct_process_dec_sp(struct operand* oper);
int instruct_process_dec_bp(struct operand* oper);
int instruct_process_dec_si(struct operand* oper);
int instruct_process_dec_di(struct operand* oper);
int instruct_process_push_ax(struct operand* oper);
int instruct_process_push_cx(struct operand* oper);
int instruct_process_push_dx(struct operand* oper);
int instruct_process_push_bx(struct operand* oper);
int instruct_process_push_sp(struct operand* oper);
int instruct_process_push_bp(struct operand* oper);
int instruct_process_push_si(struct operand* oper);
int instruct_process_push_di(struct operand* oper);
int instruct_process_pop_ax(struct operand* oper);
int instruct_process_pop_cx(struct operand* oper);
int instruct_process_pop_dx(struct operand* oper);
int instruct_process_pop_bx(struct operand* oper);
int instruct_process_pop_sp(struct operand* oper);
int instruct_process_pop_bp(struct operand* oper);
int instruct_process_pop_si(struct operand* oper);
int instruct_process_pop_di(struct operand* oper);
int instruct_process_jo_ip8(struct operand* oper);
int instruct_process_jno_ip8(struct operand* oper);
int instruct_process_jb_ip8(struct operand* oper);
int instruct_process_jnb_ip8(struct operand* oper);
int instruct_process_jz_ip8(struct operand* oper);
int instruct_process_jnz_ip8(struct operand* oper);
int instruct_process_jbe_ip8(struct operand* oper);
int instruct_process_ja_ip8(struct operand* oper);
int instruct_process_js_ip8(struct operand* oper);
int instruct_process_jns_ip8(struct operand* oper);
int instruct_process_jp_ip8(struct operand* oper);
int instruct_process_jnp_ip8(struct operand* oper);
int instruct_process_jl_ip8(struct operand* oper);
int instruct_process_jnl_ip8(struct operand* oper);
int instruct_process_jle_ip8(struct operand* oper);
int instruct_process_jnle_ip8(struct operand* oper);
int instruct_process_table_80(struct operand* oper);
int instruct_process_table_81(struct operand* oper);
int instruct_process_table_82(struct operand* oper);
int instruct_process_table_83(struct operand* oper);
int instruct_process_test_reg2rm_8(struct operand* oper);
int instruct_process_test_reg2rm_16(struct operand* oper);
int instruct_process_xchg_reg2rm_8(struct operand* oper);
int instruct_process_xchg_reg2rm_16(struct operand* oper);
int instruct_process_mov_reg2rm_8(struct operand* oper);
int instruct_process_mov_reg2rm_16(struct operand* oper);
int instruct_process_mov_rm2reg_8(struct operand* oper);
int instruct_process_mov_rm2reg_16(struct operand* oper);
int instruct_process_mov_seg2rm_16(struct operand* oper);
int instruct_process_lea_rm2reg(struct operand* oper);
int instruct_process_mov_rm2seg_16(struct operand* oper);
int instruct_process_pop_rm16(struct operand* oper);
int instruct_process_nop(struct operand* oper);
int instruct_process_xchg_cxax(struct operand* oper);
int instruct_process_xchg_dxax(struct operand* oper);
int instruct_process_xchg_bxax(struct operand* oper);
int instruct_process_xchg_spax(struct operand* oper);
int instruct_process_xchg_bpax(struct operand* oper);
int instruct_process_xchg_siax(struct operand* oper);
int instruct_process_xchg_diax(struct operand* oper);
int instruct_process_cbw(struct operand* oper);
int instruct_process_cwd(struct operand* oper);
int instruct_process_call_far(struct operand* oper);
int instruct_process_wait(struct operand* oper);
int instruct_process_pushf(struct operand* oper);
int instruct_process_popf(struct operand* oper);
int instruct_process_sahf(struct operand* oper);
int instruct_process_lahf(struct operand* oper);
int instruct_process_mov_rm2al(struct operand* oper);
int instruct_process_mov_rm2ax(struct operand* oper);
int instruct_process_mov_al2rm(struct operand* oper);
int instruct_process_mov_ax2rm(struct operand* oper);
int instruct_process_movsw_8(struct operand* oper);
int instruct_process_movsw_16(struct operand* oper);
int instruct_process_cmps_8(struct operand* oper);
int instruct_process_cmps_16(struct operand* oper);
int instruct_process_test_im8(struct operand* oper);
int instruct_process_test_im16(struct operand* oper);
int instruct_process_stos_8(struct operand* oper);
int instruct_process_stos_16(struct operand* oper);
int instruct_process_lods_8(struct operand* oper);
int instruct_process_lods_16(struct operand* oper);
int instruct_process_scas_8(struct operand* oper);
int instruct_process_scas_16(struct operand* oper);
int instruct_process_mov_im82al(struct operand* oper);
int instruct_process_mov_im82cl(struct operand* oper);
int instruct_process_mov_im82dl(struct operand* oper);
int instruct_process_mov_im82bl(struct operand* oper);
int instruct_process_mov_im82ah(struct operand* oper);
int instruct_process_mov_im82ch(struct operand* oper);
int instruct_process_mov_im82dh(struct operand* oper);
int instruct_process_mov_im82bh(struct operand* oper);
int instruct_process_mov_im162ax(struct operand* oper);
int instruct_process_mov_im162cx(struct operand* oper);
int instruct_process_mov_im162dx(struct operand* oper);
int instruct_process_mov_im162bx(struct operand* oper);
int instruct_process_mov_im162sp(struct operand* oper);
int instruct_process_mov_im162bp(struct operand* oper);
int instruct_process_mov_im162si(struct operand* oper);
int instruct_process_mov_im162di(struct operand* oper);
int instruct_process_ret_im16(struct operand* oper);
int instruct_process_ret(struct operand* oper);
int instruct_process_les_rm2reg_16(struct operand* oper);
int instruct_process_lds_rm2reg_16(struct operand* oper);
int instruct_process_mov_im2rm_8(struct operand* oper);
int instruct_process_mov_im2rm_16(struct operand* oper);
int instruct_process_retf_im16(struct operand* oper);
int instruct_process_retf(struct operand* oper);
int instruct_process_int3(struct operand* oper);
int instruct_process_int(struct operand* oper);
int instruct_process_into(struct operand* oper);
int instruct_process_iret(struct operand* oper);
int instruct_process_table_d0(struct operand* oper);
int instruct_process_table_d1(struct operand* oper);
int instruct_process_table_d2(struct operand* oper);
int instruct_process_table_d3(struct operand* oper);
int instruct_process_aam(struct operand* oper);
int instruct_process_aad(struct operand* oper);
int instruct_process_xlat(struct operand* oper);
int instruct_process_esc(struct operand* oper);
int instruct_process_loopne_8(struct operand* oper);
int instruct_process_loope_8(struct operand* oper);
int instruct_process_loop_8(struct operand* oper);
int instruct_process_jcxz_8(struct operand* oper);
int instruct_process_inal(struct operand* oper);
int instruct_process_inax(struct operand* oper);
int instruct_process_outal(struct operand* oper);
int instruct_process_outax(struct operand* oper);
int instruct_process_call_near(struct operand* oper);
int instruct_process_jmp_near_16(struct operand* oper);
int instruct_process_jmp_far(struct operand* oper);
int instruct_process_jmp_near_8(struct operand* oper);
int instruct_process_inaldx(struct operand* oper);
int instruct_process_inaxdx(struct operand* oper);
int instruct_process_outaldx(struct operand* oper);
int instruct_process_outaxdx(struct operand* oper);
int instruct_process_lock(struct operand* oper);
int instruct_process_repne(struct operand* oper);
int instruct_process_repe(struct operand* oper);
int instruct_process_halt(struct operand* oper);
int instruct_process_cmc(struct operand* oper);
int instruct_process_table_f6(struct operand* oper);
int instruct_process_table_f7(struct operand* oper);
int instruct_process_clc(struct operand* oper);
int instruct_process_stc(struct operand* oper);
int instruct_process_cli(struct operand* oper);
int instruct_process_sti(struct operand* oper);
int instruct_process_cld(struct operand* oper);
int instruct_process_std(struct operand* oper);
int instruct_process_table_fe(struct operand* oper);
int instruct_process_table_ff(struct operand* oper);

/*
 * #################指令处理函数##########################
 */

static void push_stack_16(cpu8086_core_t* core, uint16_t reg){
	assert(core && reg);

	addr_t addr = vm_addr_calc(core->reg.ss, core->reg.sp);
	vm_write_word(addr, reg);

	core->reg.sp -= 2;
}

static void pop_stack_16(cpu8086_core_t* core, uint16_t* reg){
	assert(core && reg);

	addr_t addr = vm_addr_calc(core->reg.ss, core->reg.sp);
	*reg = vm_read_word(addr);

	core->reg.sp += 2;
}

typedef int (*cpu8086_instruction_parse)(struct operand*);
typedef int (*cpu8086_instruction_proc)(struct operand*);

struct cpu8086_instruction{
	//char * level1;	//分级主要是用于部分扩展的指令，一个操作数对应多个指令操作
	//char * level2;  //数组形式，["a","b","c"]
	cpu8086_instruction_parse parse;
	cpu8086_instruction_proc  proc;
} cpu8086_instruction_table[] = {
	{parse_format_reg2rm_8, instruct_process_add_reg2rm_8}, 	//0x00
	{parse_format_reg2rm_16, instruct_process_add_reg2rm_16}, 	//0x01
	{parse_format_reg2rm_8, instruct_process_add_rm2reg_8},		//0x02
	{parse_format_reg2rm_16, instruct_process_add_rm2reg_16},	//0x03
	{parse_format_imm_8, instruct_process_add_i8al},			//0x04
	{parse_format_imm_16, instruct_process_add_i16ax},			//0x05
	{NULL, instruct_process_push_es},							//0x06
	{NULL, instruct_process_pop_es},							//0x07
	{parse_format_reg2rm_8, instruct_process_or_reg2rm_8},		//0x08
	{parse_format_reg2rm_16, instruct_process_or_reg2rm_16},	//0x09
	{parse_format_reg2rm_8, instruct_process_or_rm2reg_8},		//0x0a
	{parse_format_reg2rm_16, instruct_process_or_rm2reg_16},	//0x0b
	{parse_format_imm_8, instruct_process_or_i8al},				//0x0c
	{parse_format_imm_16, instruct_process_or_i16ax},			//0x0d
	{NULL, instruct_process_push_cs},							//0x0e
	{NULL, NULL},												//0x0f	
	{parse_format_reg2rm_8, instruct_process_adc_reg2rm_8}, 	//0x10
	{parse_format_reg2rm_16, instruct_process_adc_reg2rm_16}, 	//0x11
	{parse_format_reg2rm_8, instruct_process_adc_rm2reg_8},		//0x12
	{parse_format_reg2rm_16, instruct_process_adc_rm2reg_16},	//0x13
	{parse_format_imm_8, instruct_process_adc_i8al},			//0x14
	{parse_format_imm_16, instruct_process_adc_i16ax},			//0x15
	{NULL, instruct_process_push_ss},							//0x16
	{NULL, instruct_process_pop_ss},							//0x17
	{parse_format_reg2rm_8, instruct_process_sbb_reg2rm_8}, 	//0x18
	{parse_format_reg2rm_16, instruct_process_sbb_reg2rm_16}, 	//0x19
	{parse_format_reg2rm_8, instruct_process_sbb_rm2reg_8},		//0x1a
	{parse_format_reg2rm_16, instruct_process_sbb_rm2reg_16},	//0x1b
	{parse_format_imm_8, instruct_process_sbb_i8al},			//0x1c
	{parse_format_imm_16, instruct_process_sbb_i16ax},			//0x1d
	{NULL, instruct_process_push_ds},							//0x1e
	{NULL, instruct_process_pop_ds},							//0x1f
	{parse_format_reg2rm_8, instruct_process_and_reg2rm_8}, 	//0x20
	{parse_format_reg2rm_16, instruct_process_and_reg2rm_16}, 	//0x21
	{parse_format_reg2rm_8, instruct_process_and_rm2reg_8},		//0x22
	{parse_format_reg2rm_16, instruct_process_and_rm2reg_16},	//0x23
	{parse_format_imm_8, instruct_process_and_i8al},			//0x24
	{parse_format_imm_16, instruct_process_and_i16ax},			//0x25
	{NULL, NULL},												//0x26	
	{NULL, instruct_process_daa},								//0x27	
	{parse_format_reg2rm_8, instruct_process_sub_reg2rm_8}, 	//0x28
	{parse_format_reg2rm_16, instruct_process_sub_reg2rm_16}, 	//0x29
	{parse_format_reg2rm_8, instruct_process_sub_rm2reg_8},		//0x2a
	{parse_format_reg2rm_16, instruct_process_sub_rm2reg_16},	//0x2b
	{parse_format_imm_8, instruct_process_sub_i8al},			//0x2c
	{parse_format_imm_16, instruct_process_sub_i16ax},			//0x2d
	{NULL, NULL},												//0x2e
	{NULL, instruct_process_das},								//0x2f
	{parse_format_reg2rm_8, instruct_process_xor_reg2rm_8}, 	//0x30
	{parse_format_reg2rm_16, instruct_process_xor_reg2rm_16}, 	//0x31
	{parse_format_reg2rm_8, instruct_process_xor_rm2reg_8},		//0x32
	{parse_format_reg2rm_16, instruct_process_xor_rm2reg_16},	//0x33
	{parse_format_imm_8, instruct_process_xor_i8al},			//0x34
	{parse_format_imm_16, instruct_process_xor_i16ax},			//0x35
	{NULL, NULL},												//0x36
	{NULL, instruct_process_aaa},								//0x37
	{parse_format_reg2rm_8, instruct_process_cmp_reg2rm_8}, 	//0x38
	{parse_format_reg2rm_16, instruct_process_cmp_reg2rm_16}, 	//0x39
	{parse_format_reg2rm_8, instruct_process_cmp_rm2reg_8},		//0x3a
	{parse_format_reg2rm_16, instruct_process_cmp_rm2reg_16},	//0x3b
	{parse_format_imm_8, instruct_process_cmp_i8al},			//0x3c
	{parse_format_imm_16, instruct_process_cmp_i16ax},			//0x3d
	{NULL, NULL},												//0x3e
	{NULL, instruct_process_aas},								//0x3f
	{NULL, instruct_process_inc_ax},							//0x40
	{NULL, instruct_process_inc_cx},							//0x41
	{NULL, instruct_process_inc_dx},							//0x42
	{NULL, instruct_process_inc_bx},							//0x43
	{NULL, instruct_process_inc_sp},							//0x44
	{NULL, instruct_process_inc_bp},							//0x45
	{NULL, instruct_process_inc_si},							//0x46
	{NULL, instruct_process_inc_di},							//0x47
	{NULL, instruct_process_dec_ax},							//0x48
	{NULL, instruct_process_dec_cx},							//0x49
	{NULL, instruct_process_dec_dx},							//0x4a
	{NULL, instruct_process_dec_bx},							//0x4b
	{NULL, instruct_process_dec_sp},							//0x4c
	{NULL, instruct_process_dec_bp},							//0x4d
	{NULL, instruct_process_dec_si},							//0x4e
	{NULL, instruct_process_dec_di},							//0x4f
	{NULL, instruct_process_push_ax},							//0x50
	{NULL, instruct_process_push_cx},							//0x51
	{NULL, instruct_process_push_dx},							//0x52
	{NULL, instruct_process_push_bx},							//0x53
	{NULL, instruct_process_push_sp},							//0x54
	{NULL, instruct_process_push_bp},							//0x55
	{NULL, instruct_process_push_si},							//0x56
	{NULL, instruct_process_push_di},							//0x57
	{NULL, instruct_process_pop_ax},							//0x58
	{NULL, instruct_process_pop_cx},							//0x59
	{NULL, instruct_process_pop_dx},							//0x5a
	{NULL, instruct_process_pop_bx},							//0x5b
	{NULL, instruct_process_pop_sp},							//0x5c
	{NULL, instruct_process_pop_bp},							//0x5d
	{NULL, instruct_process_pop_si},							//0x5e
	{NULL, instruct_process_pop_di},							//0x5f
	{NULL, NULL},												//0x60
	{NULL, NULL},												//0x61
	{NULL, NULL},												//0x62
	{NULL, NULL},												//0x63
	{NULL, NULL},												//0x64
	{NULL, NULL},												//0x65
	{NULL, NULL},												//0x66
	{NULL, NULL},												//0x67
	{NULL, NULL},												//0x68
	{NULL, NULL},												//0x69
	{NULL, NULL},												//0x6a
	{NULL, NULL},												//0x6b
	{NULL, NULL},												//0x6c
	{NULL, NULL},												//0x6d
	{NULL, NULL},												//0x6e
	{NULL, NULL},												//0x6f
	{parse_format_ipinc_8, instruct_process_jo_ip8},			//0x70
	{parse_format_ipinc_8, instruct_process_jno_ip8},			//0x71
	{parse_format_ipinc_8, instruct_process_jb_ip8},			//0x72
	{parse_format_ipinc_8, instruct_process_jnb_ip8},			//0x73
	{parse_format_ipinc_8, instruct_process_jz_ip8},			//0x74
	{parse_format_ipinc_8, instruct_process_jnz_ip8},			//0x75
	{parse_format_ipinc_8, instruct_process_jbe_ip8},			//0x76
	{parse_format_ipinc_8, instruct_process_ja_ip8},			//0x77
	{parse_format_ipinc_8, instruct_process_js_ip8},			//0x78
	{parse_format_ipinc_8, instruct_process_jns_ip8},			//0x79
	{parse_format_ipinc_8, instruct_process_jp_ip8},			//0x7a
	{parse_format_ipinc_8, instruct_process_jnp_ip8},			//0x7b
	{parse_format_ipinc_8, instruct_process_jl_ip8},			//0x7c
	{parse_format_ipinc_8, instruct_process_jnl_ip8},			//0x7d
	{parse_format_ipinc_8, instruct_process_jle_ip8},			//0x7e
	{parse_format_ipinc_8, instruct_process_jnle_ip8},			//0x7f
	{parse_format_rm2imm_8, instruct_process_table_80}, 		//0x80
	{parse_format_rm2imm_16, instruct_process_table_81}, 		//0x81
	{parse_format_rm2imm_8, instruct_process_table_82}, 		//0x82
	{parse_format_rm2imm_16, instruct_process_table_83}, 		//0x83
	{parse_format_reg2rm_8, instruct_process_test_reg2rm_8}, 	//0x84
	{parse_format_reg2rm_16, instruct_process_test_reg2rm_16}, 	//0x85
	{parse_format_reg2rm_8, instruct_process_xchg_reg2rm_8}, 	//0x86
	{parse_format_reg2rm_16, instruct_process_xchg_reg2rm_16}, 	//0x87
	{parse_format_reg2rm_8, instruct_process_mov_reg2rm_8}, 	//0x88
	{parse_format_reg2rm_16, instruct_process_mov_reg2rm_16}, 	//0x89
	{parse_format_reg2rm_8, instruct_process_mov_rm2reg_8}, 	//0x8a
	{parse_format_reg2rm_16, instruct_process_mov_rm2reg_16}, 	//0x8b
	{parse_format_seg2rm_16, instruct_process_mov_seg2rm_16}, 	//0x8c
	{parse_format_reg2rm_lea, instruct_process_lea_rm2reg},		//0x8d
	{parse_format_seg2rm_16, instruct_process_mov_rm2seg_16}, 	//0x8e
	{parse_format_seg2rm_16, instruct_process_pop_rm16}, 		//0x8f
	{NULL, instruct_process_nop}, 								//0x90
	{NULL, instruct_process_xchg_cxax}, 						//0x91
	{NULL, instruct_process_xchg_dxax}, 						//0x92
	{NULL, instruct_process_xchg_bxax}, 						//0x93
	{NULL, instruct_process_xchg_spax}, 						//0x94
	{NULL, instruct_process_xchg_bpax}, 						//0x95
	{NULL, instruct_process_xchg_siax}, 						//0x96
	{NULL, instruct_process_xchg_diax}, 						//0x97
	{NULL, instruct_process_cbw}, 								//0x98
	{NULL, instruct_process_cwd}, 								//0x99
	{parse_format_call_16, instruct_process_call_far}, 			//0x9a
	{NULL, instruct_process_wait},	 							//0x9b
	{NULL, instruct_process_pushf},	 							//0x9c
	{NULL, instruct_process_popf},	 							//0x9d
	{NULL, instruct_process_sahf},	 							//0x9e
	{NULL, instruct_process_lahf},	 							//0x9f
	{parse_format_reg2rm_8, instruct_process_mov_rm2al}, 	    //0xa0
	{parse_format_reg2rm_16, instruct_process_mov_rm2ax},   	//0xa1
	{parse_format_reg2rm_8, instruct_process_mov_al2rm}, 	    //0xa2
	{parse_format_reg2rm_16, instruct_process_mov_ax2rm},   	//0xa3
	{NULL, instruct_process_movsw_8},   						//0xa4
	{NULL, instruct_process_movsw_16},   						//0xa5
	{NULL, instruct_process_cmps_8},   							//0xa6
	{NULL, instruct_process_cmps_16},   						//0xa7
	{parse_format_imm_8, instruct_process_test_im8}, 		   	//0xa8
	{parse_format_imm_16, instruct_process_test_im16}, 		   	//0xa9
	{NULL, instruct_process_stos_8},   							//0xaa
	{NULL, instruct_process_stos_16},   						//0xab
	{NULL, instruct_process_lods_8},   							//0xac
	{NULL, instruct_process_lods_16},   						//0xad
	{NULL, instruct_process_scas_8},   							//0xae
	{NULL, instruct_process_scas_16},   						//0xaf
	{parse_format_imm_8, instruct_process_mov_im82al}, 		   	//0xb0
	{parse_format_imm_8, instruct_process_mov_im82cl}, 		   	//0xb1
	{parse_format_imm_8, instruct_process_mov_im82dl}, 		   	//0xb2
	{parse_format_imm_8, instruct_process_mov_im82bl}, 		   	//0xb3
	{parse_format_imm_8, instruct_process_mov_im82ah}, 		   	//0xb4
	{parse_format_imm_8, instruct_process_mov_im82ch}, 		   	//0xb5
	{parse_format_imm_8, instruct_process_mov_im82dh}, 		   	//0xb6
	{parse_format_imm_8, instruct_process_mov_im82bh}, 		   	//0xb7
	{parse_format_imm_16, instruct_process_mov_im162ax}, 		//0xb8
	{parse_format_imm_16, instruct_process_mov_im162cx}, 		//0xb9
	{parse_format_imm_16, instruct_process_mov_im162dx}, 		//0xba
	{parse_format_imm_16, instruct_process_mov_im162bx}, 		//0xbb
	{parse_format_imm_16, instruct_process_mov_im162sp}, 		//0xbc
	{parse_format_imm_16, instruct_process_mov_im162bp}, 		//0xbd
	{parse_format_imm_16, instruct_process_mov_im162si}, 		//0xbe
	{parse_format_imm_16, instruct_process_mov_im162di}, 		//0xbf
	{NULL, NULL}, 												//0xc0
	{NULL, NULL}, 												//0xc1
	{parse_format_imm_16, instruct_process_ret_im16}, 			//0xc2
	{NULL, instruct_process_ret}, 								//0xc3
	{parse_format_reg2rm_16, instruct_process_les_rm2reg_16},	//0xc4
	{parse_format_reg2rm_16, instruct_process_lds_rm2reg_16},	//0xc5
	{parse_format_rm2imm_8, instruct_process_mov_im2rm_8},		//0xc6
	{parse_format_rm2imm_16, instruct_process_mov_im2rm_16},	//0xc7
	{NULL, NULL}, 												//0xc8
	{NULL, NULL}, 												//0xc9
	{parse_format_imm_16, instruct_process_retf_im16}, 			//0xca
	{NULL, instruct_process_retf}, 								//0xcb
	{NULL, instruct_process_int3}, 								//0xcc
	{parse_format_imm_8, instruct_process_int}, 				//0xcd
	{NULL, instruct_process_into}, 								//0xce
	{NULL, instruct_process_iret}, 								//0xcf
	{parse_format_table_rm_8, instruct_process_table_d0}, 		//0xd0
	{parse_format_table_rm_16, instruct_process_table_d1}, 		//0xd1
	{parse_format_table_rm_8, instruct_process_table_d2}, 		//0xd2
	{parse_format_table_rm_16, instruct_process_table_d3}, 		//0xd3
	{parse_format_imm_8, instruct_process_aam}, 				//0xd4
	{parse_format_imm_8, instruct_process_aad}, 				//0xd5
	{NULL, NULL}, 												//0xd6
	{NULL, instruct_process_xlat}, 							    //0xd7
	{parse_format_reg2rm_8, instruct_process_esc}, 				//0xd8
	{parse_format_reg2rm_16, instruct_process_esc}, 			//0xd9
	{parse_format_reg2rm_8, instruct_process_esc}, 				//0xda
	{parse_format_reg2rm_16, instruct_process_esc}, 			//0xdb
	{parse_format_reg2rm_8, instruct_process_esc}, 				//0xdc
	{parse_format_reg2rm_16, instruct_process_esc}, 			//0xdd
	{parse_format_reg2rm_8, instruct_process_esc}, 				//0xde
	{parse_format_reg2rm_16, instruct_process_esc}, 			//0xdf
	{parse_format_ipinc_8, instruct_process_loopne_8},			//0xe0
	{parse_format_ipinc_8, instruct_process_loope_8},			//0xe1
	{parse_format_ipinc_8, instruct_process_loop_8},			//0xe2
	{parse_format_ipinc_8, instruct_process_jcxz_8},			//0xe3
	{parse_format_imm_8, instruct_process_inal},				//0xe4
	{parse_format_imm_8, instruct_process_inax},				//0xe5
	{parse_format_imm_8, instruct_process_outal},				//0xe6
	{parse_format_imm_8, instruct_process_outax},				//0xe7
	{parse_format_ipinc_16, instruct_process_call_near},		//0xe8
	{parse_format_ipinc_16, instruct_process_jmp_near_16},		//0xe9
	{parse_format_call_16, instruct_process_jmp_far},			//0xea
	{parse_format_ipinc_8, instruct_process_jmp_near_8},		//0xeb
	{NULL, instruct_process_inaldx},							//0xec
	{NULL, instruct_process_inaxdx},							//0xed
	{NULL, instruct_process_outaldx},							//0xee
	{NULL, instruct_process_outaxdx},							//0xef
	{NULL, instruct_process_lock},								//0xf0
	{NULL, NULL},												//0xf1
	{NULL, instruct_process_repne},								//0xf2
	{NULL, instruct_process_repe},								//0xf3
	{NULL, instruct_process_halt},								//0xf4
	{NULL, instruct_process_cmc},								//0xf5
	{parse_format_table_rm_8, instruct_process_table_f6},		//0xf6
	{parse_format_table_rm_16, instruct_process_table_f7},		//0xf7
	{NULL, instruct_process_clc},								//0xf8
	{NULL, instruct_process_stc},								//0xf9
	{NULL, instruct_process_cli},								//0xfa
	{NULL, instruct_process_sti},								//0xfb
	{NULL, instruct_process_cld},								//0xfc
	{NULL, instruct_process_std},								//0xfd
	{parse_format_table_rm_8, instruct_process_table_fe},		//0xfe
	{parse_format_table_rm_16, instruct_process_table_ff}		//0xff
};

int instruct_process_add_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;
	uint8_t d = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		d = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t old = d;
	d += s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG8){
		*(uint8_t*)oper->operand2.reg = d;
	}

	vm_fprintf(stdout, "add %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_add_reg2rm_16(struct operand* oper){
	uint16_t s = *oper->operand1.reg;
	uint16_t d = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		d = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t old = d;
	d += s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG16){
		d = *oper->operand2.reg;
	}

	vm_fprintf(stdout, "add %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_add_rm2reg_8(struct operand* oper){
	uint8_t d = *(uint8_t*)oper->operand1.reg;
	uint8_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t old = d;
	d += s;

	*(uint8_t*)oper->operand1.reg = d ;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "add %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_add_rm2reg_16(struct operand* oper){
	uint16_t d = *oper->operand1.reg;
	uint16_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		s = *oper->operand2.reg;
	}

	uint16_t old = d;
	d += s;

	*oper->operand1.reg = d ;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));


	vm_fprintf(stdout, "add %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_add_i8al(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t *al = (uint8_t*)&core->reg.ax;
	uint8_t old = *al;
	uint8_t s = (uint8_t)oper->operand1.im;

	*al = *al + s;

	uint8_t d = *al;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "add al, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_add_i16ax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t *ax = &core->reg.ax;
	uint16_t old = *ax;
	uint16_t s = oper->operand1.im;

	*ax = *ax + s;

	uint16_t d = *ax;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "add ax, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_push_es(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.es);

	vm_fprintf(stdout, "push es\n");

	return 0;
}

int instruct_process_pop_es(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, &core->reg.es);

	vm_fprintf(stdout, "pop es\n");

	return 0;

}

int instruct_process_or_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;
	uint8_t d = vm_read_byte(oper->operand2.addr);

	d = d | s;

	//更新标志位 PF,ZF,SF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_write_byte(oper->operand2.addr, d);

	vm_fprintf(stdout, "or %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_or_reg2rm_16(struct operand* oper){
	uint16_t s = *oper->operand1.reg;
	uint16_t d = vm_read_word(oper->operand2.addr);

	d = d | s;

	//更新标志位 PF,ZF,SF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_write_word(oper->operand2.addr, d);

	vm_fprintf(stdout, "or %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_or_rm2reg_8(struct operand* oper){
	uint8_t d = *(uint8_t*)oper->operand1.reg;
	uint8_t s = vm_read_byte(oper->operand2.addr);

	d = d | s;

	*(uint8_t*)oper->operand1.reg = d;

	//更新标志位 PF,ZF,SF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_fprintf(stdout, "or %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_or_rm2reg_16(struct operand* oper){
	uint16_t d = *oper->operand1.reg;
	uint16_t s = vm_read_word(oper->operand2.addr);

	d = d | s;

	*oper->operand1.reg = d;

	//更新标志位 PF,ZF,SF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_fprintf(stdout, "or %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_or_i8al(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t *al = (uint8_t*)&core->reg.ax;
	uint8_t s = (uint8_t)oper->operand1.im;

	*al = *al | s;

	uint8_t d = *al;

	//更新标志位 PF,ZF,SF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_fprintf(stdout, "or al, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_or_i16ax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t *ax = &core->reg.ax;
	uint16_t s = oper->operand1.im;

	*ax = *ax | s;

	uint16_t d = *ax;

	//更新标志位 PF,ZF,SF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_fprintf(stdout, "or ax, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_push_cs(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.cs);

	vm_fprintf(stdout, "push cs\n");

	return 0;
}

int instruct_process_adc_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;
	uint8_t d = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		d = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t old = d;
	d = d + s + FLAGS_CF(core);

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, d);
	} else {
		*(uint8_t*)oper->operand2.reg = d;
	}

	vm_fprintf(stdout, "adc %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_adc_reg2rm_16(struct operand* oper){
	uint16_t s = *oper->operand1.reg;
	uint16_t d = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		d = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t old = d;
	d = d + s + FLAGS_CF(core);

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG16){
		*oper->operand2.reg = d;
	}

	vm_fprintf(stdout, "adc %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;

}

int instruct_process_adc_rm2reg_8(struct operand* oper){
	uint8_t d = *(uint8_t*)oper->operand1.reg;
	uint8_t s = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t old = d;
	d = d + s + FLAGS_CF(core);

	*(uint8_t*)oper->operand1.reg = d ;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "adc %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;

}

int instruct_process_adc_rm2reg_16(struct operand* oper){
	uint16_t d = *oper->operand1.reg;
	uint16_t s = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t old = d;
	d = d + s + FLAGS_CF(core);

	*oper->operand1.reg = d ;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));


	vm_fprintf(stdout, "adc %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_adc_i8al(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t *al = (uint8_t*)&core->reg.ax;
	uint8_t old = *al;
	uint8_t s = (uint8_t)oper->operand1.im;

	*al = *al + s + FLAGS_CF(core);

	uint8_t d = *al;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "adc al, %s\n", oper->alias_operand1);

	return 0;

}

int instruct_process_adc_i16ax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t *ax = &core->reg.ax;
	uint16_t old = *ax;
	uint16_t s = oper->operand1.im;

	*ax = *ax + s + FLAGS_CF(core);

	uint16_t d = *ax;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "adc ax, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_push_ss(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint16_t oldsp = core->reg.sp;
	core->reg.sp -= 2;

	addr_t addr = vm_addr_calc(core->reg.ss, oldsp);
	vm_write_word(addr, core->reg.ss);

	vm_fprintf(stdout, "push ss\n");

	return 0;

}

int instruct_process_pop_ss(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, &core->reg.ss);

	vm_fprintf(stdout, "pop ss\n");

	return 0;
}


int instruct_process_sbb_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;
	uint8_t d = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		d = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t old = d;
	d = d - s - FLAGS_CF(core);

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG8){
		*(uint8_t*)oper->operand2.reg = d;
	}

	vm_fprintf(stdout, "sbb %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_sbb_reg2rm_16(struct operand* oper){
	uint16_t s = *oper->operand1.reg;
	uint16_t d = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		d = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t old = d;
	d = d - s - FLAGS_CF(core);

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG16){
		*oper->operand2.reg = d;
	}

	vm_fprintf(stdout, "sbb %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;

}

int instruct_process_sbb_rm2reg_8(struct operand* oper){
	uint8_t d = *(uint8_t*)oper->operand1.reg;
	uint8_t s = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t old = d;
	d = d - s - FLAGS_CF(core);

	*(uint8_t*)oper->operand1.reg = d ;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "sbb %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_sbb_rm2reg_16(struct operand* oper){
	uint16_t d = *oper->operand1.reg;
	uint16_t s = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t old = d;
	d = d - s - FLAGS_CF(core);

	*oper->operand1.reg = d ;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "sbb %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_sbb_i8al(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t *al = (uint8_t*)&core->reg.ax;
	uint8_t old = *al;
	uint8_t s = (uint8_t)oper->operand1.im;

	*al = *al - s - FLAGS_CF(core);

	uint8_t d = *al;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "sbb al, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_sbb_i16ax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t *ax = &core->reg.ax;
	uint16_t old = *ax;
	uint16_t s = oper->operand1.im;

	*ax = *ax - s - FLAGS_CF(core);

	uint16_t d = *ax;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "sbb ax, %s\n", oper->alias_operand1);

	return 0;

}

int instruct_process_push_ds(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.ds);

	vm_fprintf(stdout, "push ds\n");

	return 0;
}

int instruct_process_pop_ds(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, core->reg.ds);

	vm_fprintf(stdout, "pop ds\n");

	return 0;
}

int instruct_process_and_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;
	uint8_t d = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		d = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	d = d & s;

	//更新标志位 CF,PF,ZF,SF,OF
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG8){
		*(uint8_t*)oper->operand2.reg = d;
	}

	vm_fprintf(stdout, "and %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_and_reg2rm_16(struct operand* oper){
	uint16_t s = *oper->operand1.reg;
	uint16_t d = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		d = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t old = d;
	d = d & s;

	//更新标志位 CF,PF,ZF,SF,OF
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG16){
		*oper->operand2.reg = d;
	}

	vm_fprintf(stdout, "and %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_and_rm2reg_8(struct operand* oper){
	uint8_t d = *(uint8_t*)oper->operand1.reg;
	uint8_t s = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	d = d & s;

	*(uint8_t*)oper->operand1.reg = d ;

	//更新标志位 CF,PF,ZF,SF,OF
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);

	vm_fprintf(stdout, "and %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_and_rm2reg_16(struct operand* oper){
	uint16_t d = *oper->operand1.reg;
	uint16_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	d = d & s;

	*oper->operand1.reg = d ;

	//更新标志位 CF,PF,AF,SF,OF
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);

	vm_fprintf(stdout, "and %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;

}

int instruct_process_and_i8al(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t *al = (uint8_t*)&core->reg.ax;
	uint8_t s = (uint8_t)oper->operand1.im;

	*al = *al & s;

	uint8_t d = *al;

	//更新标志位 CF,PF,AF,SF,OF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_fprintf(stdout, "and al, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_and_i16ax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t *ax = &core->reg.ax;
	uint16_t s = oper->operand1.im;

	*ax = *ax & s;

	uint16_t d = core->reg.ax;

	//更新标志位 CF,PF,AF,SF,OF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_fprintf(stdout, "and ax, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_daa(struct operand* oper){
	uint8_t al_l4, al_h4;

	cpu8086_core_t * core = get_core();
	uint8_t* al = (uint8_t*)&core->reg.ax;

	al_l4 = *al & 0x000f;
	al_h4 = (*al & 0x00f0) >> 4;

	if(al_l4 > 9 || FLAGS_AF(core) == 1){
		*al += 0x6;
		update_flags(FLAGS_A, 1);
	}else {
		update_flags(FLAGS_A, 0);
	}

	if(al_h4 > 9 || FLAGS_CF(core) == 1){
		*al += 0x60;
		update_flags(FLAGS_C, 1);
	} else {
		update_flags(FLAGS_C, 0);
	}

	vm_fprintf(stdout, "daa\n");

	return 0;
}


int instruct_process_sub_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;
	uint8_t d = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		d = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t old = d;
	d = d - s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG8){
		*(uint8_t*)oper->operand2.reg = d;
	}

	vm_fprintf(stdout, "sub %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_sub_reg2rm_16(struct operand* oper){
	uint16_t s = *oper->operand1.reg;
	uint16_t d = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		d = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t old = d;
	d = d - s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG16){
		*oper->operand2.reg = d;
	}

	vm_fprintf(stdout, "sub %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_sub_rm2reg_8(struct operand* oper){
	uint8_t d = *(uint8_t*)oper->operand1.reg;
	uint8_t s = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t old = d;
	d = d - s;

	*(uint8_t*)oper->operand1.reg = d ;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "sub %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_sub_rm2reg_16(struct operand* oper){
	uint16_t d = *oper->operand1.reg;
	uint16_t s = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t old = d;
	d = d - s;

	*oper->operand1.reg = d ;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "sub %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_sub_i8al(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t *al = (uint8_t*)&core->reg.ax;
	uint8_t old = *al;
	uint8_t s = (uint8_t)oper->operand1.im;

	*al = *al - s;

	uint8_t d = *al;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "sub al, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_sub_i16ax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t *ax = &core->reg.ax;
	uint16_t old = *ax;
	uint16_t s = oper->operand1.im;

	*ax = *ax - s;

	uint16_t d = *ax;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "sub ax, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_das(struct operand* oper){
	uint8_t al_l4, al_h4;

	cpu8086_core_t * core = get_core();
	uint8_t* al = (uint8_t*)&core->reg.ax;

	al_l4 = *al & 0x000f;
	al_h4 = (*al & 0x00f0) >> 4;

	uint16_t oldax = core->reg.ax;

	if(al_l4 > 9 || FLAGS_AF(core) == 1){
		*al -= 6;
		update_flags(FLAGS_A, 1);
	} else {
		update_flags(FLAGS_A, 0);
	}

	if(al_h4 > 9 || FLAGS_CF(core) == 1){
		*al -= 0x60;
		update_flags(FLAGS_C, 1);
	} else {
		update_flags(FLAGS_C, 0);
	}

	vm_fprintf(stdout, "das\n");

	return 0;
}

int instruct_process_xor_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;
	uint8_t d = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		d = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	d = d ^ s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, 0);
	update_flags(FLAGS_C, 0);

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG16){
		*(uint8_t*)oper->operand2.reg = d;
	} 

	vm_fprintf(stdout, "xor %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_xor_reg2rm_16(struct operand* oper){
	uint16_t s = *oper->operand1.reg;
	uint16_t d = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		d = *oper->operand2.reg;
	} else {
		return -1;
	}

	d = d ^ s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, 0);
	update_flags(FLAGS_C, 0);

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, d);
	} else if(oper->operand2_type == OPERAND_REG16){
		*oper->operand2.reg = d;
	}

	vm_fprintf(stdout, "xor %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;

}

int instruct_process_xor_rm2reg_8(struct operand* oper){
	uint8_t d = *(uint8_t*)oper->operand1.reg;
	uint8_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	d = d ^ s;

	*(uint8_t*)oper->operand1.reg = d;

	//更新标志位 PF,ZF,SF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_fprintf(stdout, "xor %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_xor_rm2reg_16(struct operand* oper){
	uint16_t d = *oper->operand1.reg;
	uint16_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	d = d ^ s;

	*oper->operand1.reg = d;

	//更新标志位 PF,ZF,SF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_fprintf(stdout, "xor %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;

}

int instruct_process_xor_i8al(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t *al = (uint8_t*)&core->reg.ax;
	uint8_t s = (uint8_t)oper->operand1.im;

	*al = *al ^ s;

	uint8_t d = *al;

	//更新标志位 CF,PF,AF,SF,OF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_fprintf(stdout, "xor al, %s\n", oper->alias_operand1);

	return 0;

}

int instruct_process_xor_i16ax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t *ax = &core->reg.ax;
	uint16_t s = oper->operand1.im;

	*ax = *ax ^ s;

	uint16_t d = core->reg.ax;

	//更新标志位 CF,PF,AF,SF,OF
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);

	vm_fprintf(stdout, "xor ax, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_aaa(struct operand* oper){
	uint8_t al_l4, al_h4;

	cpu8086_core_t * core = get_core();
	uint8_t *al = (uint8_t*)&core->reg.ax;
	uint8_t *ah = al + 1;

	al_l4 = *al & 0x000f;
	al_h4 = (*al & 0x00f0) >> 4;

	uint8_t carry = 0;
	uint16_t oldax = core->reg.ax;

	if(al_l4 > 9 || FLAGS_AF(core) == 1){
		*al += 0x6;
		*ah += 0x1;

		update_flags(FLAGS_A, 1);
		update_flags(FLAGS_C, 1);
	} else {
		update_flags(FLAGS_A, 0);
		update_flags(FLAGS_C, 0);
	}

	vm_fprintf(stdout, "aaa\n");

	return 0;
}

int instruct_process_cmp_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;
	uint8_t d = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		d = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t old = d;
	d = d - s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "cmp %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_cmp_reg2rm_16(struct operand* oper){
	uint16_t s = *oper->operand1.reg;
	uint16_t d = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		d = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t old = d;
	d = d - s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "cmp %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_cmp_rm2reg_8(struct operand* oper){
	uint8_t d = *(uint8_t*)oper->operand1.reg;
	uint8_t s = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t old = d;
	d = d - s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "cmp %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;

}

int instruct_process_cmp_rm2reg_16(struct operand* oper){
	uint16_t d = *oper->operand1.reg;
	uint16_t s = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t old = d;
	d = d - s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "cmp %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;

}

int instruct_process_cmp_i8al(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t d = (uint8_t*)&core->reg.ax;
	uint8_t old = d;
	uint8_t s = (uint8_t)oper->operand1.im;

	d = d - s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	vm_fprintf(stdout, "cmp al, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_cmp_i16ax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t d = core->reg.ax;
	uint16_t old = d;
	uint16_t s = oper->operand1.im;

	d = d - s;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "cmp ax, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_aas(struct operand* oper){
	uint8_t al_l4, al_h4;

	cpu8086_core_t * core = get_core();
	uint8_t* al = (uint8_t*)&core->reg.ax;
	uint8_t* ah = al + 1;

	al_l4 = *al & 0x0f;
	al_h4 = (*al & 0xf0) >> 4;

	uint8_t carry = 0;
	uint16_t oldax = core->reg.ax;

	if(al_l4 > 9 || FLAGS_AF(core) == 1){
		*al = *al - 6;
		*al = *al & 0x0f;
		*ah = *ah - 1;

		update_flags(FLAGS_A, 1);
		update_flags(FLAGS_C, 1);
	} else {
		update_flags(FLAGS_A, 0);
		update_flags(FLAGS_C, 0);
	}


	vm_fprintf(stdout, "aas\n");

	return 0;
}

int instruct_process_inc_ax(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* ax = &core->reg.ax;
	uint16_t s = 1;

	uint16_t old = *ax;
	*ax = *ax + s;

	uint16_t d = *ax;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "inc ax\n");

	return 0;
}

int instruct_process_inc_cx(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* cx = &core->reg.cx;
	uint16_t s = 1;

	uint16_t old = *cx;
	*cx = *cx + s;

	uint16_t d = *cx;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "inc cx\n");

	return 0;
}

int instruct_process_inc_dx(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* dx = &core->reg.dx;
	uint16_t s = 1;

	uint16_t old = *dx;
	*dx = *dx + s;

	uint16_t d = *dx;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "inc dx\n");

	return 0;
}

int instruct_process_inc_bx(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* bx = &core->reg.bx;
	uint16_t s = 1;

	uint16_t old = *bx;
	*bx = *bx + s;

	uint16_t d = *bx;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "inc bx\n");

	return 0;
}

int instruct_process_inc_sp(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* sp = &core->reg.sp;
	uint16_t s = 1;

	uint16_t old = *sp;
	*sp = *sp + s;

	uint16_t d = *sp;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "inc sp\n");

	return 0;
}

int instruct_process_inc_bp(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* bp = &core->reg.bp;
	uint16_t s = 1;

	uint16_t old = *bp;
	*bp = *bp + s;

	uint16_t d = *bp;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "inc bp\n");

	return 0;
}

int instruct_process_inc_si(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* si = &core->reg.si;
	uint16_t s = 1;

	uint16_t old = *si;
	*si = *si + s;

	uint16_t d = *si;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "inc si\n");

	return 0;
}

int instruct_process_inc_di(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* di = &core->reg.di;
	uint16_t s = 1;

	uint16_t old = *di;
	*di = *di + s;

	uint16_t d = *di;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old > d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "inc di\n");

	return 0;
}

int instruct_process_dec_ax(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* ax = &core->reg.ax;
	uint16_t s = 1;

	uint16_t old = *ax;
	*ax = *ax - s;

	uint16_t d = *ax;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "dec ax\n");

	return 0;
}

int instruct_process_dec_cx(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* cx = &core->reg.cx;
	uint16_t s = 1;

	uint16_t old = *cx;
	*cx = *cx - s;

	uint16_t d = *cx;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "dec cx\n");

	return 0;
}

int instruct_process_dec_dx(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* dx = &core->reg.dx;
	uint16_t s = 1;

	uint16_t old = *dx;
	*dx = *dx - s;

	uint16_t d = *dx;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "dec dx\n");

	return 0;
}

int instruct_process_dec_bx(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* bx = &core->reg.bx;
	uint16_t s = 1;

	uint16_t old = *bx;
	*bx = *bx - s;

	uint16_t d = *bx;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "dec bx\n");

	return 0;
}

int instruct_process_dec_sp(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* sp = &core->reg.sp;
	uint16_t s = 1;

	uint16_t old = *sp;
	*sp = *sp - s;

	uint16_t d = *sp;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "dec sp\n");

	return 0;
}

int instruct_process_dec_bp(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* bp = &core->reg.bp;
	uint16_t s = 1;

	uint16_t old = *bp;
	*bp = *bp - s;

	uint16_t d = *bp;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "dec bp\n");

	return 0;
}

int instruct_process_dec_si(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* si = &core->reg.si;
	uint16_t s = 1;

	uint16_t old = *si;
	*si = *si - s;

	uint16_t d = *si;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "dec si\n");

	return 0;
}

int instruct_process_dec_di(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* di = &core->reg.di;
	uint16_t s = 1;

	uint16_t old = *di;
	*di = *di - s;

	uint16_t d = *di;

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	vm_fprintf(stdout, "dec di\n");

	return 0;
}

int instruct_process_push_ax(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.ax);

	vm_fprintf(stdout, "push ax\n");

	return 0;
}

int instruct_process_push_cx(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.cx);

	vm_fprintf(stdout, "push cx\n");

	return 0;
}

int instruct_process_push_dx(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.dx);

	vm_fprintf(stdout, "push dx\n");

	return 0;
}

int instruct_process_push_bx(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.bx);

	vm_fprintf(stdout, "push bx\n");

	return 0;
}

int instruct_process_push_sp(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.sp);

	vm_fprintf(stdout, "push sp\n");

	return 0;
}

int instruct_process_push_bp(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.bp);

	vm_fprintf(stdout, "push bp\n");

	return 0;
}

int instruct_process_push_si(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.si);

	vm_fprintf(stdout, "push si\n");

	return 0;
}

int instruct_process_push_di(struct operand* oper){
	cpu8086_core_t * core = get_core();

	push_stack_16(core, core->reg.di);

	vm_fprintf(stdout, "push di\n");

	return 0;
}

int instruct_process_pop_ax(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, &core->reg.ax);

	vm_fprintf(stdout, "pop ax\n");

	return 0;
}

int instruct_process_pop_cx(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, &core->reg.cx);

	vm_fprintf(stdout, "pop cx\n");

	return 0;
}

int instruct_process_pop_dx(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, &core->reg.dx);

	vm_fprintf(stdout, "pop dx\n");

	return 0;
}

int instruct_process_pop_bx(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, &core->reg.bx);

	vm_fprintf(stdout, "pop bx\n");

	return 0;
}

int instruct_process_pop_sp(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, &core->reg.sp);

	vm_fprintf(stdout, "pop sp\n");

	return 0;
}

int instruct_process_pop_bp(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, &core->reg.bp);

	vm_fprintf(stdout, "pop bp\n");

	return 0;
}

int instruct_process_pop_si(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, &core->reg.si);

	vm_fprintf(stdout, "pop si\n");

	return 0;
}

int instruct_process_pop_di(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pop_stack_16(core, &core->reg.di);

	vm_fprintf(stdout, "pop di\n");

	return 0;
}

int instruct_process_jo_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if(FLAGS_OF(core) == 1){
		core->reg.ip += offset;
	}

	vm_fprintf(stdout, "jo %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jno_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if(FLAGS_OF(core) == 0 || FLAGS_ZF(core) == 1){
		core->reg.ip += offset;
	}

	vm_fprintf(stdout, "jno %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jb_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if(FLAGS_CF(core) == 1){
		core->reg.ip += offset;
	}

	vm_fprintf(stdout, "jb %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jnb_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if(FLAGS_CF(core) == 0 || FLAGS_ZF(core) == 1){
		core->reg.ip += offset;
	}

	vm_fprintf(stdout, "jnb %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jz_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if(FLAGS_ZF(core) == 1){
		core->reg.ip += offset;
	}

	vm_fprintf(stdout, "jz %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jnz_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if(FLAGS_ZF(core) == 0){
		core->reg.ip += offset;
	}

	vm_fprintf(stdout, "jnz %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jbe_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if((FLAGS_CF(core) == 1) || (FLAGS_ZF(core) == 1)){
		core->reg.ip += offset;
	}

	vm_fprintf(stdout, "jbe %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_ja_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if((FLAGS_CF(core) == 0) || (FLAGS_ZF(core) == 0))
		core->reg.ip += offset;

	vm_fprintf(stdout, "ja %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_js_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if(FLAGS_SF(core) == 1)
		core->reg.ip += offset;

	vm_fprintf(stdout, "js %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jns_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if(FLAGS_SF(core) == 0)
		core->reg.ip += offset;

	vm_fprintf(stdout, "jns %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jp_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if(FLAGS_PF(core) == 1)
		core->reg.ip += offset;

	vm_fprintf(stdout, "jp %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jnp_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if(FLAGS_PF(core) == 0)
		core->reg.ip += offset;

	vm_fprintf(stdout, "jnp %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jl_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if((FLAGS_ZF(core) == 0) && ((FLAGS_SF(core) == 1) && (FLAGS_OF(core) == 1)))
		core->reg.ip += offset;

	vm_fprintf(stdout, "jl %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jnl_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if((FLAGS_ZF(core) == 1) || ((FLAGS_SF(core) == 0) && (FLAGS_OF(core) == 0)))
		core->reg.ip += offset;

	vm_fprintf(stdout, "jnl %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_jle_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if((FLAGS_ZF(core) == 1) || ((FLAGS_SF(core) == 1) && (FLAGS_OF(core) == 1)))
		core->reg.ip += offset;

	vm_fprintf(stdout, "jle %s\n",oper->alias_operand1);

	return 0;

}

int instruct_process_jnle_ip8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	int8_t offset = (int8_t)oper->operand1.offset;

	if((FLAGS_ZF(core) == 0) && ((FLAGS_SF(core) == 0) && (FLAGS_OF(core) == 0)))
		core->reg.ip += offset;

	vm_fprintf(stdout, "jnle %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_table_80(struct operand* oper){
	uint8_t op = oper->operand1.op;
	uint8_t d = 0;
	uint8_t s = oper->operand3.im;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16) {
		uint8_t* reg = (uint8_t*)oper->operand2.reg;

		d = *reg;
	} else {
		vm_fprintf(stderr, "unknow 80 instruction operand type\n");
		return -1;
	}

	uint8_t old;
	switch(op){
		case 0: //ADD
			old = d;
			d = d + s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old > d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "add %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 1:	//OR
			d = d | s;

			//更新标志位 PF,ZF,SF
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_C, 0);
			update_flags(FLAGS_O, 0);

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "or %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 2:	//ADC
			old = d;
			cpu8086_core_t * core = get_core();

			d = d + s + FLAGS_CF(core);

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old > d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "adc %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 3:	//SBB
			old = d;

			d = d - s - FLAGS_CF(core);

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "sbb %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 4: //AND
			d = d & s;

			//更新标志位 CF,PF,ZF,SF,OF
			update_flags(FLAGS_C, 0);
			update_flags(FLAGS_O, 0);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "and %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 5: //SUB
			old = d;

			d = d - s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "sub %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 6: //XOR
			d = d ^ s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, 0);
			update_flags(FLAGS_C, 0);

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "xor %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 7: //CMP
			d = d - s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

			vm_fprintf(stdout, "cmp %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
	}
}

int instruct_process_table_81(struct operand* oper){
	uint8_t op = oper->operand1.op;
	uint16_t d = 0;
	uint16_t s = oper->operand3.im;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16) {
		uint16_t* reg = oper->operand2.reg;

		d = *reg;
	} else {
		vm_fprintf(stderr, "unknow 81 instruction operand type\n");
		return -1;
	}

	uint16_t old;
	switch(op){
		case 0: //ADD
			old = d;
			d = d + s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old > d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "add %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 1:	//OR
			d = d | s;

			//更新标志位 PF,ZF,SF
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_C, 0);
			update_flags(FLAGS_O, 0);

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "or %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 2:	//ADC
			old = d;
			cpu8086_core_t * core = get_core();

			d = d + s + FLAGS_CF(core);

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old > d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "adc %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 3:	//SBB
			old = d;

			d = d - s - FLAGS_CF(core);

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "sbb %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 4: //AND
			d = d & s;

			//更新标志位 CF,PF,ZF,SF,OF
			update_flags(FLAGS_C, 0);
			update_flags(FLAGS_O, 0);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "and %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 5: //SUB
			old = d;

			d = d - s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "sub %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 6: //XOR
			d = d ^ s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, 0);
			update_flags(FLAGS_C, 0);

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "xor %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 7: //CMP
			d = d - s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

			vm_fprintf(stdout, "cmp %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
	}
}

int instruct_process_table_82(struct operand* oper){
	uint8_t op = oper->operand1.op;
	uint8_t d = 0;
	uint8_t s = oper->operand3.im;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16) {
		uint8_t* reg = (uint8_t*)oper->operand2.reg;

		d = *reg;
	} else {
		vm_fprintf(stderr, "unknow 82 instruction operand type\n");
		return -1;
	}

	uint8_t old;
	switch(op){
		case 0: //ADD
			old = d;
			d = d + s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old > d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "add %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 2:	//ADC
			old = d;
			cpu8086_core_t * core = get_core();

			d = d + s + FLAGS_CF(core);

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old > d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "adc %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 3:	//SBB
			old = d;

			d = d - s - FLAGS_CF(core);

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "sbb %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 5: //SUB
			old = d;

			d = d - s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint8_t* reg = (uint8_t*)oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "sub %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 7: //CMP
			d = d - s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

			vm_fprintf(stdout, "cmp %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		default:
			vm_fprintf(stderr,"82 instruction operand code not support\n");
			return -1;
	}
}

int instruct_process_table_83(struct operand* oper){
	uint8_t op = oper->operand1.op;
	uint16_t d = 0;
	uint16_t s = oper->operand3.im;

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16) {
		uint16_t* reg = oper->operand2.reg;

		d = *reg;
	} else {
		vm_fprintf(stderr, "unknow 81 instruction operand type\n");
		return -1;
	}

	uint16_t old;
	switch(op){
		case 0: //ADD
			old = d;
			d = d + s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old > d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "add %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 2:	//ADC
			old = d;
			cpu8086_core_t * core = get_core();

			d = d + s + FLAGS_CF(core);

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old > d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) + (s & 0x0f)) > 0x0f);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "adc %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 3:	//SBB
			old = d;

			d = d - s - FLAGS_CF(core);

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "sbb %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 5: //SUB
			old = d;

			d = d - s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

			if(oper->operand2_type == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			} else if(oper->operand2_type == OPERAND_REG16) {
				uint16_t* reg = oper->operand2.reg;
				*reg = d;
			}

			vm_fprintf(stdout, "sub %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		case 7: //CMP
			d = d - s;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, old < d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

			vm_fprintf(stdout, "cmp %s, %s\n", oper->alias_operand2, oper->alias_operand3);

			break;
		default:
			vm_fprintf(stderr,"83 instruction operand code not support\n");
			return -1;
	}
}

int instruct_process_test_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;
	uint8_t d = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		d = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	d = d & s;

	//更新标志位 CF,PF,ZF,SF,OF
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);

	vm_fprintf(stdout, "test %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_test_reg2rm_16(struct operand* oper){
	uint16_t s = *oper->operand1.reg;
	uint16_t d = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		d = *oper->operand2.reg;
	} else {
		return -1;
	}

	d = d & s;

	//更新标志位 CF,PF,ZF,SF,OF
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);

	vm_fprintf(stdout, "test %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_xchg_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;
	uint8_t d = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG8){
		d = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	*(uint8_t*)oper->operand1.reg = d;
	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, s);
	} else if(oper->operand2_type == OPERAND_REG8){
		*(uint8_t*)oper->operand2.reg = s;
	}

	vm_fprintf(stdout, "xchg %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_xchg_reg2rm_16(struct operand* oper){
	uint16_t s = *(uint16_t*)oper->operand1.reg;
	uint16_t d = 0;
	cpu8086_core_t * core = get_core();

	if(oper->operand2_type == OPERAND_ADDR){
		d = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		d = *oper->operand2.reg;
	} else {
		return -1;
	}

	*oper->operand1.reg = d;
	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, s);
	} else if(oper->operand2_type == OPERAND_REG16){
		*oper->operand2.reg = s;
	}

	vm_fprintf(stdout, "xchg %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_mov_reg2rm_8(struct operand* oper){
	uint8_t s = *(uint8_t*)oper->operand1.reg;

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, s);
	} else if(oper->operand2_type == OPERAND_REG8){
		*(uint8_t*)oper->operand2.reg = s;
	} else {
		return -1;
	}

	vm_fprintf(stdout, "mov %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_mov_reg2rm_16(struct operand* oper){
	uint16_t s = *oper->operand1.reg;

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, s);
	} else if(oper->operand2_type == OPERAND_REG16){
		*oper->operand2.reg = s;
	} else {
		return -1;
	}

	vm_fprintf(stdout, "mov %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_mov_rm2reg_8(struct operand* oper){
	uint8_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	*(uint8_t*)oper->operand1.reg = s;

	vm_fprintf(stdout, "mov %s, %s\n", oper->alias_operand1, oper->alias_operand2);
}

int instruct_process_mov_rm2reg_16(struct operand* oper){
	uint16_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	*oper->operand1.reg = s;

	vm_fprintf(stdout, "mov %s, %s\n", oper->alias_operand1, oper->alias_operand2);
}

int instruct_process_mov_seg2rm_16(struct operand* oper){
	uint16_t seg = *oper->operand1.reg;

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, seg);
	} else if(oper->operand2_type == OPERAND_REG16){
		*oper->operand2.reg = seg;
	} else {
		return -1;
	}

	vm_fprintf(stdout, "mov %s, %s\n", oper->alias_operand2, oper->alias_operand1);

	return 0;
}

int instruct_process_lea_rm2reg(struct operand* oper){
	uint16_t *reg = oper->operand1.reg;

	if(oper->operand2_type != OPERAND_IM16){
		return -1;
	}

	*reg = oper->operand2.im;

	vm_fprintf(stdout, "lea %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_mov_rm2seg_16(struct operand* oper){
	uint16_t* seg = oper->operand1.reg;
	uint16_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	} else if(oper->operand2_type == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	*seg = s;

	vm_fprintf(stdout, "mov %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_pop_rm16(struct operand* oper){
	uint16_t s = 0;

	cpu8086_core_t *core = get_core();

	uint16_t oldsp = core->reg.sp;
	core->reg.sp += 2;

	addr_t addr = vm_addr_calc(core->reg.ss, oldsp);
	s = vm_read_word(addr);

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, s);
	} else if(oper->operand2_type == OPERAND_REG16){
		*oper->operand2.reg = s;
	} else {
		return -1;
	}

	vm_fprintf(stdout, "pop %s\n",oper->alias_operand2);

	return 0;
}

int instruct_process_nop(struct operand* oper){
	vm_fprintf(stdout, "nop\n");

	return 0;
}

int instruct_process_xchg_cxax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t* ax = &core->reg.ax;
	uint16_t* cx = &core->reg.cx;

	SWAP(*ax, *cx);

	vm_fprintf(stdout, "xchg ax, cx\n");

	return 0;
}

int instruct_process_xchg_dxax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t* ax = &core->reg.ax;
	uint16_t* dx = &core->reg.dx;

	SWAP(*ax, *dx);

	vm_fprintf(stdout, "xchg ax, dx\n");

	return 0;
}

int instruct_process_xchg_bxax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t* ax = &core->reg.ax;
	uint16_t* bx = &core->reg.bx;

	SWAP(*ax, *bx);

	vm_fprintf(stdout, "xchg ax, bx\n");

	return 0;
}

int instruct_process_xchg_spax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t* ax = &core->reg.ax;
	uint16_t* sp = &core->reg.sp;

	SWAP(*ax, *sp);

	vm_fprintf(stdout, "xchg ax, sp\n");

	return 0;
}

int instruct_process_xchg_bpax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t* ax = &core->reg.ax;
	uint16_t* bp = &core->reg.bp;

	SWAP(*ax, *bp);

	vm_fprintf(stdout, "xchg ax, bp\n");

	return 0;
}

int instruct_process_xchg_siax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t* ax = &core->reg.ax;
	uint16_t* si = &core->reg.si;

	SWAP(*ax, *si);

	vm_fprintf(stdout, "xchg ax, si\n");

	return 0;
}

int instruct_process_xchg_diax(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t* ax = &core->reg.ax;
	uint16_t* di = &core->reg.di;

	SWAP(*ax, *di);

	vm_fprintf(stdout, "xchg ax, di\n");

	return 0;
}

int instruct_process_cbw(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t* al = (uint8_t*)&core->reg.ax;
	uint8_t* ah = al + 1;

	if((*al & 0x80) == 1){
		*ah = 0xff;
	} else {
		*ah = 0x00;
	}

	vm_fprintf(stdout, "cbw\n");

	return 0;
}

int instruct_process_cwd(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t  ax = core->reg.ax;
	uint16_t* dx = &core->reg.dx;

	if((ax & 0x8000) == 1){
		*dx = 0xffff;
	} else {
		*dx = 0x0000;
	}

	vm_fprintf(stdout, "cwd\n");

	return 0;
}

int instruct_process_call_far(struct operand* oper){
	uint16_t offset = oper->operand1.offset;
	uint16_t segment = oper->operand2.segment;

	cpu8086_core_t* core = get_core();

	uint16_t oldsp = core->reg.sp;
	addr_t addr = vm_addr_calc(core->reg.ss, oldsp);
	vm_write_word(addr, core->reg.ip);
	core->reg.sp -= 2;

	addr-= 2;
	vm_write_word(addr, core->reg.cs);

	core->reg.cs = segment;
	core->reg.ip = offset;

	vm_fprintf(stdout,"call far %04x:%04x\n",core->reg.cs, core->reg.ip);

	return 0;
}

int instruct_process_wait(struct operand* oper){
	//do nothing
	vm_fprintf(stdout, "wait\n");
	return 0;
}

int instruct_process_pushf(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint16_t oldsp = core->reg.sp;
	core->reg.sp -= 2;

	addr_t addr = vm_addr_calc(core->reg.ss, oldsp);
	vm_write_word(addr, core->reg.flags);

	vm_fprintf(stdout, "pushf\n");

	return 0;
}

int instruct_process_popf(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint16_t oldsp = core->reg.sp;
	core->reg.sp += 2;

	addr_t addr = vm_addr_calc(core->reg.ss, oldsp);
	core->reg.flags = vm_read_word(addr);

	vm_fprintf(stdout, "popf\n");

	return 0;
}

int instruct_process_sahf(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t ax = core->reg.ax;
	uint8_t ah = (uint8_t)(ax >> 8);

	uint16_t *flags = &core->reg.flags;

	*(uint8_t*)flags = ah;

	vm_fprintf(stdout, "sahf\n");

	return 0;
}

int instruct_process_lahf(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint8_t* al = (uint8_t*)&core->reg.ax;
	uint8_t* ah = al + 1;

	uint16_t flags = core->reg.flags;

	*ah = (uint8_t)flags;

	vm_fprintf(stdout, "lahf\n");

	return 0;
}

int instruct_process_mov_rm2al(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint8_t* al = (uint8_t*)&core->reg.ax;
	uint8_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	} else {
		return -1;
	}

	*al = s;

	vm_fprintf(stdout, "mov al, %s\n",oper->alias_operand2);

	return 0;
}

int instruct_process_mov_rm2ax(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t* ax = &core->reg.ax;
	uint16_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	} else {
		return -1;
	}

	*ax = s;

	vm_fprintf(stdout, "mov ax, %s\n",oper->alias_operand2);

	return 0;
}

int instruct_process_mov_al2rm(struct operand* oper){
	uint8_t* al = (uint8_t*)oper->operand2.reg;
	uint16_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, *al);
	} else {
		return -1;
	}

	vm_fprintf(stdout, "mov %s, al\n",oper->alias_operand2);

	return 0;
}

int instruct_process_mov_ax2rm(struct operand* oper){
	uint16_t* ax = oper->operand2.reg;
	uint16_t s = 0;

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, *ax);
	} else {
		return -1;
	}

	vm_fprintf(stdout, "mov %s, ax\n",oper->alias_operand2);

	return 0;
}

int instruct_process_movsw_8(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t si = core->reg.si;
	uint16_t di = core->reg.di;
	uint16_t ds = core->reg.ds;
	uint16_t es = core->reg.es;

	addr_t s = vm_addr_calc(ds, si);
	addr_t d = vm_addr_calc(es, di);

	uint8_t sb = vm_read_byte(s);

	vm_write_byte(d, sb);

	if(FLAGS_DF(core) == 0){
		core->reg.si++;
		core->reg.di++;
	} else {
		core->reg.si--;
		core->reg.di--;
	}

	vm_fprintf(stdout,"movsb\n");

	return 0;
}

int instruct_process_movsw_16(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t si = core->reg.si;
	uint16_t di = core->reg.di;
	uint16_t ds = core->reg.ds;
	uint16_t es = core->reg.es;

	addr_t s = vm_addr_calc(ds, si);
	addr_t d = vm_addr_calc(es, di);

	uint16_t sw = vm_read_word(s);

	vm_write_word(d, sw);

	if(FLAGS_DF(core) == 0){
		core->reg.si+=2;
		core->reg.di+=2;
	} else {
		core->reg.si-=2;
		core->reg.di-=2;
	}

	vm_fprintf(stdout,"movsw\n");

	return 0;
}

int instruct_process_cmps_8(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t si = core->reg.si;
	uint16_t di = core->reg.di;
	uint16_t ds = core->reg.ds;
	uint16_t es = core->reg.es;

	addr_t sa = vm_addr_calc(ds, si);
	addr_t da = vm_addr_calc(es, di);

	uint8_t s = vm_read_byte(sa);
	uint8_t d = vm_read_byte(da);

	uint8_t old = d;
	d = d - s; 

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	if(FLAGS_DF(core) == 0){
		core->reg.si++;
		core->reg.di++;
	} else {
		core->reg.si--;
		core->reg.di--;
	}

	vm_fprintf(stdout,"cmpsb\n");

	return 0;
}

int instruct_process_cmps_16(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t si = core->reg.si;
	uint16_t di = core->reg.di;
	uint16_t ds = core->reg.ds;
	uint16_t es = core->reg.es;

	addr_t sa = vm_addr_calc(ds, si);
	addr_t da = vm_addr_calc(es, di);

	uint16_t s = vm_read_word(sa);
	uint16_t d = vm_read_word(da);

	uint16_t old = d;
	d = d - s; 

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	if(FLAGS_DF(core) == 0){
		core->reg.si++;
		core->reg.di++;
	} else {
		core->reg.si--;
		core->reg.di--;
	}

	vm_fprintf(stdout,"cmpsw\n");

	return 0;
}

int instruct_process_test_im8(struct operand* oper){
	uint8_t im = (uint8_t)oper->operand1.im;
	cpu8086_core_t *core = get_core();

	uint8_t al = (uint8_t)core->reg.ax;

	uint8_t d = al & im;

	//更新标志位 CF,PF,ZF,SF,OF
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);

	vm_fprintf(stdout,"test al, %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_test_im16(struct operand* oper){
	uint16_t im = oper->operand1.im;
	cpu8086_core_t *core = get_core();

	uint16_t ax = core->reg.ax;

	uint16_t d = ax & im;

	//更新标志位 CF,PF,ZF,SF,OF
	update_flags(FLAGS_C, 0);
	update_flags(FLAGS_O, 0);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);

	vm_fprintf(stdout,"test ax, %s\n",oper->alias_operand1);

	return 0;
}

int instruct_process_stos_8(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t al = (uint8_t)core->reg.ax;
	uint16_t di = core->reg.di;
	uint16_t es = core->reg.es;

	addr_t addr = vm_addr_calc(es, di);

	vm_write_byte(addr, al);

	if(FLAGS_DF(core) == 0){
		core->reg.si++;
		core->reg.di++;
	} else {
		core->reg.si--;
		core->reg.di--;
	}

	vm_fprintf(stdout,"stosb\n");

	return 0;
}

int instruct_process_stos_16(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t ax = core->reg.ax;
	uint16_t di = core->reg.di;
	uint16_t es = core->reg.es;

	addr_t addr = vm_addr_calc(es, di);

	vm_write_word(addr, ax);

	if(FLAGS_DF(core) == 0){
		core->reg.si++;
		core->reg.di++;
	} else {
		core->reg.si--;
		core->reg.di--;
	}

	vm_fprintf(stdout,"stosw\n");

	return 0;
}

int instruct_process_lods_8(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint8_t* al = (uint8_t*)&core->reg.ax;
	uint16_t di = core->reg.di;
	uint16_t es = core->reg.es;

	addr_t addr = vm_addr_calc(es, di);

	*al = vm_read_byte(addr);

	if(FLAGS_DF(core) == 0){
		core->reg.si++;
		core->reg.di++;
	} else {
		core->reg.si--;
		core->reg.di--;
	}

	vm_fprintf(stdout,"lodsb\n");

	return 0;
}

int instruct_process_lods_16(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t* ax = &core->reg.ax;
	uint16_t di = core->reg.di;
	uint16_t es = core->reg.es;

	addr_t addr = vm_addr_calc(es, di);

	*ax = vm_read_word(addr);

	if(FLAGS_DF(core) == 0){
		core->reg.si++;
		core->reg.di++;
	} else {
		core->reg.si--;
		core->reg.di--;
	}

	vm_fprintf(stdout,"lodsw\n");

	return 0;
}

int instruct_process_scas_8(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t si = core->reg.si;
	uint16_t di = core->reg.di;
	uint16_t ds = core->reg.ds;
	uint16_t es = core->reg.es;

	addr_t sa = vm_addr_calc(ds, si);
	addr_t da = vm_addr_calc(es, di);

	uint8_t s = vm_read_byte(sa);
	uint8_t d = vm_read_byte(da);

	uint8_t old = s;
	d = s - d; 

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 7);
	update_flags(FLAGS_O, (d & 0x80) ^ (old & 0x80));

	if(FLAGS_DF(core) == 0){
		core->reg.si++;
		core->reg.di++;
	} else {
		core->reg.si--;
		core->reg.di--;
	}

	vm_fprintf(stdout,"scasb\n");

	return 0;
}

int instruct_process_scas_16(struct operand* oper){
	cpu8086_core_t *core = get_core();

	uint16_t si = core->reg.si;
	uint16_t di = core->reg.di;
	uint16_t ds = core->reg.ds;
	uint16_t es = core->reg.es;

	addr_t sa = vm_addr_calc(ds, si);
	addr_t da = vm_addr_calc(es, di);

	uint16_t s = vm_read_word(sa);
	uint16_t d = vm_read_word(da);

	uint16_t old = s;
	d = s - d; 

	//更新标志位 CF,PF,AF,ZF,SF,OF
	update_flags(FLAGS_C, old < d);
	update_flags(FLAGS_P, d % 2 == 0);
	update_flags(FLAGS_A, ((old & 0x0f) < (s & 0x0f)));
	update_flags(FLAGS_Z, d == 0);
	update_flags(FLAGS_S, d >> 15);
	update_flags(FLAGS_O, (d & 0x8000) ^ (old & 0x8000));

	if(FLAGS_DF(core) == 0){
		core->reg.si++;
		core->reg.di++;
	} else {
		core->reg.si--;
		core->reg.di--;
	}

	vm_fprintf(stdout,"scasw\n");

	return 0;
}

int instruct_process_mov_im82al(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint8_t *al = (uint8_t*)&core->reg.ax;

	*al = (uint8_t)oper->operand1.im;

	vm_fprintf(stdout, "mov al, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im82cl(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint8_t *cl = (uint8_t*)&core->reg.cx;

	*cl = (uint8_t)oper->operand1.im;

	vm_fprintf(stdout, "mov cl, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im82dl(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint8_t *dl = (uint8_t*)&core->reg.dx;

	*dl = (uint8_t)oper->operand1.im;

	vm_fprintf(stdout, "mov dl, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im82bl(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint8_t *bl = (uint8_t*)&core->reg.bx;

	*bl = (uint8_t)oper->operand1.im;

	vm_fprintf(stdout, "mov bl, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im82ah(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint8_t *ah = ((uint8_t*)&core->reg.ax) + 1;

	*ah = (uint8_t)oper->operand1.im;

	vm_fprintf(stdout, "mov ah, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im82ch(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint8_t *ch = ((uint8_t*)&core->reg.cx) + 1;

	*ch = (uint8_t)oper->operand1.im;

	vm_fprintf(stdout, "mov ch, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im82dh(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint8_t *dh = ((uint8_t*)&core->reg.dx) + 1;

	*dh = (uint8_t)oper->operand1.im;

	vm_fprintf(stdout, "mov dh, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im82bh(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint8_t *bh = ((uint8_t*)&core->reg.dx) + 1;

	*bh = (uint8_t)oper->operand1.im;

	vm_fprintf(stdout, "mov bh, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im162ax(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint16_t *ax = &core->reg.ax;

	*ax = oper->operand1.im;

	vm_fprintf(stdout, "mov ax, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im162cx(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint16_t *cx = &core->reg.cx;

	*cx = oper->operand1.im;

	vm_fprintf(stdout, "mov cx, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im162dx(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint16_t *dx = &core->reg.dx;

	*dx = oper->operand1.im;

	vm_fprintf(stdout, "mov dx, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im162bx(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint16_t *bx = &core->reg.bx;

	*bx = oper->operand1.im;

	vm_fprintf(stdout, "mov bx, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im162sp(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint16_t *sp = &core->reg.sp;

	*sp = oper->operand1.im;

	vm_fprintf(stdout, "mov sp, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im162bp(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint16_t *bp = &core->reg.bp;

	*bp = oper->operand1.im;

	vm_fprintf(stdout, "mov bp, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im162si(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint16_t *si = &core->reg.si;

	*si = oper->operand1.im;

	vm_fprintf(stdout, "mov si, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_mov_im162di(struct operand* oper){
	cpu8086_core_t* core = get_core();

	uint16_t *di = &core->reg.di;

	*di = oper->operand1.im;

	vm_fprintf(stdout, "mov di, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_ret_im16(struct operand* oper){
	uint16_t im = oper->operand1.im;
	cpu8086_core_t *core = get_core();

	core->reg.ip = im;

	vm_fprintf(stdout, "ret %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_ret(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint16_t oldsp = core->reg.sp;
	core->reg.sp += 2;

	addr_t addr = vm_addr_calc(core->reg.ss, oldsp);
	uint16_t offset = vm_read_word(addr);

	core->reg.ip = offset;

	vm_fprintf(stdout, "ret\n", oper->alias_operand1);

	return 0;
}

int instruct_process_les_rm2reg_16(struct operand* oper){
	uint16_t* reg = oper->operand1.reg;

	if(oper->operand2_type != OPERAND_ADDR){
		return -1;
	}

	uint32_t im = vm_read_dword(oper->operand2.addr);

	uint16_t im_l = (uint16_t)im;
	uint16_t im_h = (uint16_t)(im >> 16);

	*reg = im_l;

	cpu8086_core_t* core = get_core();
	core->reg.es = im_h;

	vm_fprintf(stdout, "les %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_lds_rm2reg_16(struct operand* oper){
	uint16_t* reg = oper->operand1.reg;

	if(oper->operand2_type != OPERAND_ADDR){
		return -1;
	}

	uint32_t im = vm_read_dword(oper->operand2.addr);

	uint16_t im_l = (uint16_t)im;
	uint16_t im_h = (uint16_t)(im >> 16);

	*reg = im_l;

	cpu8086_core_t* core = get_core();
	core->reg.ds = im_h;

	vm_fprintf(stdout, "lds %s, %s\n", oper->alias_operand1, oper->alias_operand2);

	return 0;
}

int instruct_process_mov_im2rm_8(struct operand* oper){
	if(oper->operand1.op != 0){
		return -1;
	}

	uint8_t im = (uint8_t)oper->operand3.im;

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, im);
	} else if(oper->operand2_type == OPERAND_REG8){
		*(uint8_t*)oper->operand2.reg = im;
	} else {
		return -1;
	}

	vm_fprintf(stdout, "mov %s, %s\n", oper->alias_operand2, oper->alias_operand3);

	return 0;
}

int instruct_process_mov_im2rm_16(struct operand* oper){
	if(oper->operand1.op != 0){
		return -1;
	}

	uint16_t im = oper->operand3.im;

	if(oper->operand2_type == OPERAND_ADDR){
		vm_write_word(oper->operand2.addr, im);
	} else if(oper->operand2_type == OPERAND_REG16){
		*oper->operand2.reg = im;
	} else {
		return -1;
	}

	vm_fprintf(stdout, "mov %s, %s\n", oper->alias_operand2, oper->alias_operand3);

	return 0;
}

int instruct_process_retf_im16(struct operand* oper){
	uint16_t im = oper->operand1.im;
	cpu8086_core_t *core = get_core();

	addr_t addr = vm_addr_calc(core->reg.ss, core->reg.sp);
	core->reg.sp += 2;

	core->reg.cs = vm_read_word(addr);

	core->reg.ip = im;

	vm_fprintf(stdout, "ret %s\n", oper->alias_operand1);

	return 0;

}

int instruct_process_retf(struct operand* oper){
	cpu8086_core_t * core = get_core();

	addr_t addr = vm_addr_calc(core->reg.ss, core->reg.sp);
	uint16_t offset = vm_read_word(addr);

	core->reg.ip = offset;
	core->reg.cs = vm_read_word(addr + 2);

	core->reg.sp += 4;

	vm_fprintf(stdout, "ret\n", oper->alias_operand1);

	return 0;

}

int instruct_process_int3(struct operand* oper){
	cpu8086_core_t* core = get_core();

	push_stack_16(core, core->reg.flags);
	FLAGS_TF_SET(core, 0);
	FLAGS_IF_SET(core, 0);
	push_stack_16(core, core->reg.cs);
	push_stack_16(core, core->reg.ip);

	core->reg.cs = (uint16_t)3 * 4 + 2;
	core->reg.ip = (uint16_t)3 * 4;

	vm_fprintf(stdout, "int3\n");

	return 0;
}

int instruct_process_int(struct operand* oper){
	uint8_t im = (uint8_t)oper->operand1.im;
	cpu8086_core_t* core = get_core();

	push_stack_16(core, core->reg.flags);
	FLAGS_TF_SET(core, 0);
	FLAGS_IF_SET(core, 0);
	push_stack_16(core, core->reg.cs);
	push_stack_16(core, core->reg.ip);

	core->reg.cs = (uint16_t)im * 4 + 2;
	core->reg.ip = (uint16_t)im * 4;

	vm_fprintf(stdout, "int %d\n", im);
}

int instruct_process_into(struct operand* oper){
	cpu8086_core_t* core = get_core();

	push_stack_16(core, core->reg.flags);
	FLAGS_TF_SET(core, 0);
	FLAGS_IF_SET(core, 0);
	push_stack_16(core, core->reg.cs);
	push_stack_16(core, core->reg.ip);

	core->reg.cs = (uint16_t)4 * 4 + 2;
	core->reg.ip = (uint16_t)4 * 4;

	vm_fprintf(stdout, "into\n");

	return 0;
}

int instruct_process_iret(struct operand* oper){
	cpu8086_core_t* core = get_core();

	push_stack_16(core, core->reg.ip);
	push_stack_16(core, core->reg.cx);
	push_stack_16(core, core->reg.flags);

	vm_fprintf(stdout, "iret\n");

	return 0;
}

int instruct_process_table_d0(struct operand* oper){
	uint8_t op = 0;
	cpu8086_core_t* core = get_core();

	if(oper->operand1_type != OPERAND_OP){
		return -1;
	}

	op = oper->operand1.op;

	uint8_t s = 0;
	if(oper->operand2.op == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t f;
	switch(op){
		case 0:	//ROL
			update_flags(FLAGS_C, !!(s & 0x80));
			s = s << 1 + FLAGS_CF(core);
			vm_fprintf(stdout,"rol %s, 1\n", oper->alias_operand2);
			break;
		case 1:	//ROR
			update_flags(FLAGS_C, !!(s & 0x01));
			s = s >> 1 + FLAGS_CF(core) << 7;

			vm_fprintf(stdout,"ror %s, 1\n", oper->alias_operand2);
			break;
		case 2:	//RCL
			f = FLAGS_CF(core);
			update_flags(FLAGS_C, !!(s & 0x80));
			s = s << 1 + f;

			vm_fprintf(stdout,"rcl %s, 1\n", oper->alias_operand2);
			break;
		case 3:	//RCR
			f = FLAGS_CF(core);
			update_flags(FLAGS_C, !!(s & 0x01));
			s = s >> 1 + f;

			vm_fprintf(stdout,"rcr %s, 1\n", oper->alias_operand2);
			break;
		case 4:	//SAL
			f = !!(s & 0x80);
			update_flags(FLAGS_C, f);
			s = s << 1;

			update_flags(FLAGS_O, f != (!!(s & 0x80)));

			vm_fprintf(stdout,"sal %s, 1\n", oper->alias_operand2);
			break;
		case 5:	//SHR
			f = !!(s & 0x80);

			update_flags(FLAGS_C, !!(s & 0x01));
			s = s >> 1;

			update_flags(FLAGS_O, f != (!!(s & 0x80)));

			vm_fprintf(stdout,"shl %s, 1\n", oper->alias_operand2);
			break;
		case 6:	//not used
			return -1;
		case 7:	//SAR
			f = !!(s & 0x80);
			update_flags(FLAGS_C, !!(s & 0x01));
			s = s >> 1;

			s = s & (f << 7);

			vm_fprintf(stdout,"sar %s, 1\n", oper->alias_operand2);
			break;
	}

	if(oper->operand2.op == OPERAND_ADDR){
		*(uint8_t*)oper->operand2.reg = vm_read_byte(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG8){
		*(uint8_t*)oper->operand2.reg = s;
	}

	return 0;
}


int instruct_process_table_d1(struct operand* oper){
	uint8_t op = 0;
	cpu8086_core_t* core = get_core();

	if(oper->operand1_type != OPERAND_OP){
		return -1;
	}

	op = oper->operand1.op;

	uint16_t s = 0;
	if(oper->operand2.op == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG8){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t f = 0;
	switch(op){
		case 0:	//ROL
			update_flags(FLAGS_C, !!(s & 0x8000));
			s = (s << 1) + FLAGS_CF(core);
			vm_fprintf(stdout,"rol %s, 1\n", oper->alias_operand2);
			break;
		case 1:	//ROR
			update_flags(FLAGS_C, !!(s & 0x0001));
			s = (s >> 1) + (FLAGS_CF(core) << 15);

			vm_fprintf(stdout,"ror %s, 1\n", oper->alias_operand2);
			break;
		case 2:	//RCL
			f = FLAGS_CF(core);
			update_flags(FLAGS_C, !!(s & 0x8000));
			s = (s << 1) + f;

			vm_fprintf(stdout,"rcl %s, 1\n", oper->alias_operand2);
			break;
		case 3:	//RCR
			f = FLAGS_CF(core);
			update_flags(FLAGS_C, !!(s & 0x0001));
			s = (s >> 1) + (f << 15);

			vm_fprintf(stdout,"rcr %s, 1\n", oper->alias_operand2);
			break;
		case 4:	//SAL
			f = !!(s & 0x8000);
			update_flags(FLAGS_C, f);
			s = s << 1;

			update_flags(FLAGS_O, f != (!!(s & 0x8000)));

			vm_fprintf(stdout,"sal %s, 1\n", oper->alias_operand2);
			break;
		case 5:	//SHR
			f = !!(s & 0x8000);

			update_flags(FLAGS_C, !!(s & 0x0001));
			s = s >> 1;

			update_flags(FLAGS_O, f != (!!(s & 0x8000)));

			vm_fprintf(stdout,"shl %s, 1\n", oper->alias_operand2);
			break;
		case 6:	//not used
			return -1;
		case 7:	//SAR
			f = !!(s & 0x8000);
			update_flags(FLAGS_C, !!(s & 0x0001));
			s = s >> 1;

			s = s & (f << 15);

			vm_fprintf(stdout,"sar %s, 1\n", oper->alias_operand2);
			break;
	}

	if(oper->operand2.op == OPERAND_ADDR){
		*oper->operand2.reg = vm_read_word(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG16){
		*oper->operand2.reg = s;
	}

	return 0;
}

int instruct_process_table_d2(struct operand* oper){
	uint8_t op = 0;
	cpu8086_core_t* core = get_core();

	if(oper->operand1_type != OPERAND_OP){
		return -1;
	}

	op = oper->operand1.op;

	uint8_t s = 0;
	if(oper->operand2.op == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t off = (uint8_t)core->reg.cx;
	if(off > 8){
		s = 0;
		update_flags(FLAGS_C, 0);
		update_flags(FLAGS_O, 0);
		goto I_TABLE_D2;
	} else if(off == 0){
		goto I_TABLE_D2;
	}

	uint8_t f;
	uint8_t f1;
	uint8_t f2;
	switch(op){
		case 0:	//ROL
			f = !!(s & (0x80 >> (off - 1)));
			update_flags(FLAGS_C, f);
			s = (s << off) + f;
			vm_fprintf(stdout,"rol %s, %d\n", oper->alias_operand2, off);
			break;
		case 1:	//ROR
			f = !!(s & (0x01 << (off - 1)));
			update_flags(FLAGS_C, f);
			s = (s >> off) + (f << 7);

			vm_fprintf(stdout,"ror %s, %d\n", oper->alias_operand2, off);
			break;
		case 2:	//RCL
			f = FLAGS_CF(core);
			f2 = !!(s & (0x80 >> (off - 1)));

			update_flags(FLAGS_C, f2);
			s = (s << off) + f2;

			vm_fprintf(stdout,"rcl %s, %d\n", oper->alias_operand2, off);
			break;
		case 3:	//RCR
			f = FLAGS_CF(core);
			f2 = !!(s & (0x01 << (off - 1)));

			update_flags(FLAGS_C, f2);
			s = (s >> off) + (f2 << 7);

			vm_fprintf(stdout,"rcr %s, %d\n", oper->alias_operand2, off);
			break;
		case 4:	//SAL
			f = !!(s & (0x80 >> (off - 1)));
			f1 = !!(s & 0x80);
			update_flags(FLAGS_C, f);
			s = s << off;

			f2 = !!(s & 0x80);
			update_flags(FLAGS_O, f1 != f2);

			vm_fprintf(stdout,"sal %s, %d\n", oper->alias_operand2, off);
			break;
		case 5:	//SHR
			f = !!(s & (0x01 << (off - 1)));
			f1 = !!(s & 0x80);

			update_flags(FLAGS_C, f);
			s = s >> off;

			f2 = !!(s & 0x80);

			update_flags(FLAGS_O, f1 != f2);

			vm_fprintf(stdout,"shl %s, %d\n", oper->alias_operand2, off);
			break;
		case 6:	//not used
			return -1;
		case 7:	//SAR
			f = !!(s & (0x01 << (off - 1)));
			update_flags(FLAGS_C, f);
			s = s >> off;

			s = s & (f << 7);

			vm_fprintf(stdout,"sar %s, %d\n", oper->alias_operand2, off);
			break;
	}

I_TABLE_D2:
	if(oper->operand2.op == OPERAND_ADDR){
		*(uint8_t*)oper->operand2.reg = vm_read_byte(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG8){
		*(uint8_t*)oper->operand2.reg = s;
	}


	return 0;
}

int instruct_process_table_d3(struct operand* oper){
	uint8_t op = 0;
	cpu8086_core_t* core = get_core();

	if(oper->operand1_type != OPERAND_OP){
		return -1;
	}

	op = oper->operand1.op;

	uint16_t s = 0;
	if(oper->operand2.op == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t off = (uint8_t)core->reg.cx;
	if(off > 16){
		s = 0;
		update_flags(FLAGS_C, 0);
		update_flags(FLAGS_O, 0);
		goto I_TABLE_D3;
	} else if(off == 0){
		goto I_TABLE_D3;
	}

	uint8_t f;
	uint8_t f1;
	uint8_t f2;
	switch(op){
		case 0:	//ROL
			f = !!(s & (0x8000 >> (off - 1)));
			update_flags(FLAGS_C, f);
			s = (s << off) + f;
			vm_fprintf(stdout,"rol %s, %d\n", oper->alias_operand2, off);
			break;
		case 1:	//ROR
			f = !!(s & (0x0001 << (off - 1)));
			update_flags(FLAGS_C, f);
			s = (s >> off) + (f << 15);

			vm_fprintf(stdout,"ror %s, %d\n", oper->alias_operand2, off);
			break;
		case 2:	//RCL
			f = FLAGS_CF(core);
			f2 = !!(s & (0x8000 >> (off - 1)));

			update_flags(FLAGS_C, f2);
			s = (s << off) + f2;

			vm_fprintf(stdout,"rcl %s, %d\n", oper->alias_operand2, off);
			break;
		case 3:	//RCR
			f = FLAGS_CF(core);
			f2 = !!(s & (0x0001 << (off - 1)));

			update_flags(FLAGS_C, f2);
			s = (s >> off) + (f2 << 15);

			vm_fprintf(stdout,"rcr %s, %d\n", oper->alias_operand2, off);
			break;
		case 4:	//SAL
			f = !!(s & (0x8000 >> (off - 1)));
			f1 = !!(s & 0x8000);
			update_flags(FLAGS_C, f);
			s = s << off;

			f2 = !!(s & 0x8000);
			update_flags(FLAGS_O, f1 != f2);

			vm_fprintf(stdout,"sal %s, %d\n", oper->alias_operand2, off);
			break;
		case 5:	//SHR
			f = !!(s & (0x0001 << (off - 1)));
			f1 = !!(s & 0x8000);

			update_flags(FLAGS_C, f);
			s = s >> off;

			f2 = !!(s & 0x8000);

			update_flags(FLAGS_O, f1 != f2);

			vm_fprintf(stdout,"shl %s, %d\n", oper->alias_operand2, off);
			break;
		case 6:	//not used
			return -1;
		case 7:	//SAR
			f = !!(s & (0x0001 << (off - 1)));
			update_flags(FLAGS_C, f);
			s = s >> off;

			s = s & (f << 15);

			vm_fprintf(stdout,"sar %s, %d\n", oper->alias_operand2, off);
			break;
	}

I_TABLE_D3:
	if(oper->operand2.op == OPERAND_ADDR){
		*oper->operand2.reg = vm_read_byte(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG8){
		*oper->operand2.reg = s;
	}


	return 0;
}

int instruct_process_aam(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint8_t al = (uint8_t)core->reg.ax;

	uint8_t a = al / 10;
	uint8_t b = al % 10;

	uint16_t c = ((uint16_t)a) << 8 + (uint16_t)b;

	core->reg.ax = c;

	update_flags(FLAGS_Z, c == 0);
	update_flags(FLAGS_P, (c % 2) != 0);
	update_flags(FLAGS_S, !!(c & 0x8000));

	vm_fprintf(stdout, "aam\n");

	return 0;
}

int instruct_process_aad(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint8_t al = (uint8_t)core->reg.ax;
	uint8_t ah = (uint8_t)(core->reg.ax >> 8);

	uint8_t a = ah * 10 + al;
	uint8_t b = 0;

	uint16_t c = ((uint16_t)b) << 8 + (uint16_t)a;

	core->reg.ax = c;

	update_flags(FLAGS_Z, c == 0);
	update_flags(FLAGS_P, (c % 2) != 0);
	update_flags(FLAGS_S, !!(c & 0x8000));

	vm_fprintf(stdout, "aad\n");

	return 0;
}

int instruct_process_xlat(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint16_t ds = core->reg.ds;
	uint16_t bx = core->reg.bx;
	uint16_t ax = core->reg.ax;

	addr_t addr = vm_addr_calc(ds, bx + (ax & 0x00ff));
	uint8_t al = vm_read_byte(addr);

	core->reg.ax = ((core->reg.ax & 0xff00) | al);

	vm_fprintf(stdout, "xlat\n");

	return 0;
}

int instruct_process_esc(struct operand* oper){
	vm_fprintf(stdout, "Do not support ESC intruction! Do nothing !\n");

	return 0;
}

int instruct_process_loopne_8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	if(oper->operand1_type != OPERAND_IP8){
		return -1;
	}

	core->reg.cx--;

	if(core->reg.cx != 0 && FLAGS_ZF(core) != 1){
		core->reg.ip = (uint16_t)oper->operand1.offset;
	}

	vm_fprintf(stdout, "loopne %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_loope_8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	if(oper->operand1_type != OPERAND_IP8){
		return -1;
	}

	core->reg.cx--;

	if(core->reg.cx != 0 && FLAGS_ZF(core) == 1){
		core->reg.ip = (uint16_t)oper->operand1.offset;
	}

	vm_fprintf(stdout, "loope %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_loop_8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	if(oper->operand1_type != OPERAND_IP8){
		return -1;
	}

	core->reg.cx--;

	if(core->reg.cx != 0){
		core->reg.ip = (uint16_t)oper->operand1.offset;
	}

	vm_fprintf(stdout, "loop %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_jcxz_8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	if(oper->operand1_type != OPERAND_IP8){
		return -1;
	}

	if(core->reg.cx == 0){
		core->reg.ip = (uint16_t)oper->operand1.offset;
	}

	vm_fprintf(stdout, "jcxz %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_inal(struct operand* oper){
	cpu8086_core_t * core = get_core();

	core->reg.ax = core->reg.ax & 0xff00;
	core->reg.ax |= (uint16_t)pci_in_byte(oper->operand1.im);

	vm_fprintf(stdout, "in al, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_inax(struct operand* oper){
	cpu8086_core_t * core = get_core();

	core->reg.ax = pci_in_word(oper->operand1.im);

	vm_fprintf(stdout, "in ax, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_outal(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pci_out_byte(oper->operand1.im, (uint8_t)core->reg.ax);

	vm_fprintf(stdout, "out al, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_outax(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pci_out_word(oper->operand1.im, core->reg.ax);

	vm_fprintf(stdout, "out ax, %s\n", oper->alias_operand1);

	return 0;
}

int instruct_process_call_near(struct operand* oper){
	uint16_t offset = oper->operand1.offset;

	cpu8086_core_t* core = get_core();

	uint16_t oldsp = core->reg.sp;
	addr_t addr = vm_addr_calc(core->reg.ss, oldsp);
	vm_write_word(addr, core->reg.ip);
	core->reg.sp -= 2;

	addr-= 2;
	vm_write_word(addr, core->reg.cs);

	core->reg.ip = offset;

	vm_fprintf(stdout,"call near %04x\n", core->reg.ip);

	return 0;
}

int instruct_process_jmp_near_16(struct operand* oper){
	uint16_t offset = oper->operand1.offset;

	cpu8086_core_t* core = get_core();
	core->reg.ip = offset;

	vm_fprintf(stdout,"jmp near %04x\n", core->reg.ip);

	return 0;
}

int instruct_process_jmp_far(struct operand* oper){
	uint16_t offset = oper->operand1.offset;
	uint16_t segment = oper->operand2.segment;

	cpu8086_core_t* core = get_core();
	core->reg.cs = segment;
	core->reg.ip = offset;

	vm_fprintf(stdout,"jmp far %04x:%04x\n",core->reg.cs, core->reg.ip);

	return 0;
}

int instruct_process_jmp_near_8(struct operand* oper){
	uint16_t offset = oper->operand1.offset;

	cpu8086_core_t* core = get_core();
	core->reg.ip = offset;

	vm_fprintf(stdout,"jmp near %02x\n", core->reg.ip);

	return 0;
}

int instruct_process_inaldx(struct operand* oper){
	cpu8086_core_t * core = get_core();

	core->reg.ax = core->reg.ax & 0xff00;
	core->reg.ax |= pci_in_byte(core->reg.dx);

	vm_fprintf(stdout, "in al, dx\n");

	return 0;
}

int instruct_process_inaxdx(struct operand* oper){
	cpu8086_core_t * core = get_core();

	core->reg.ax = core->reg.ax & 0xff00;
	core->reg.ax |= pci_in_word(core->reg.dx);

	vm_fprintf(stdout, "in ax, dx\n");

	return 0;
}

int instruct_process_outaldx(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pci_out_byte(core->reg.cx, (uint8_t)core->reg.ax);

	vm_fprintf(stdout, "out al, dx\n");

	return 0;
}

int instruct_process_outaxdx(struct operand* oper){
	cpu8086_core_t * core = get_core();

	pci_out_word(core->reg.cx, core->reg.ax);

	vm_fprintf(stdout, "out ax, dx\n");

	return 0;
}

//static pthread_mutex_t cpu_lock = PTHREAD_MUTEX_INITIALIZER;
int instruct_process_lock(struct operand* oper){
	vm_fprintf(stdout, "lock\n");

	return 0;
}

int instruct_process_repne(struct operand* opers){
	cpu8086_core_t * core = get_core();

	int nbyteproc = 0;
	int oldip = core->reg.ip;

	vm_fprintf(stdout, "repne\n");

	do {
		core->reg.ip = oldip;

		cpu8086_proc_instruction();
		
		core->reg.cx--;
	} while(core->reg.cx != 0 || FLAGS_ZF(core) == 0);

	return 0;
}

int instruct_process_repe(struct operand* opers){
	cpu8086_core_t * core = get_core();

	int nbyteproc = 0;
	int oldip = core->reg.ip;

	vm_fprintf(stdout, "repe\n");

	do {
		core->reg.ip = oldip;

		cpu8086_proc_instruction();

		core->reg.cx--;

	} while(core->reg.cx != 0 || FLAGS_ZF(core) == 1);

	return 0;
}

int instruct_process_halt(struct operand* oper){
	cpu8086_core_t * core = get_core();

	while(core->halt == 0){
		;
	}

	vm_fprintf(stdout, "halt\n");

	return 0;
}

int instruct_process_cmc(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint8_t cf = FLAGS_CF(core);

	cf = !cf;
	update_flags(FLAGS_C, cf);

	vm_fprintf(stdout, "cmc\n");

	return 0;
}

int instruct_process_table_f6(struct operand* oper){
	uint8_t op = 0;
	cpu8086_core_t* core = get_core();

	if(oper->operand1_type != OPERAND_OP){
		return -1;
	}

	op = oper->operand1.op;

	uint8_t s = 0;
	if(oper->operand2.op == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t im;
	uint8_t d;
	uint16_t d16;
	uint16_t ax;
	uint8_t ah;
	uint8_t al;
	switch(op){
		case 0:	//TEST
			im = instruct_read_byte();
			d = s & im;

			//更新标志位 CF,PF,ZF,SF,OF
			update_flags(FLAGS_C, 0);
			update_flags(FLAGS_O, 0);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);

			vm_fprintf(stdout,"test %s, %d\n", oper->alias_operand2, im);
			break;
		case 1:	//not used
			break;
		case 2:	//NOT
			d = ~s;

			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);

			if(oper->operand2.op == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			}else if(oper->operand2.op == OPERAND_REG8){
				*(uint8_t*)oper->operand2.reg = d;
			}

			vm_fprintf(stdout,"not %s\n", oper->alias_operand2);
			break;
		case 3:	//NEG
			d = 0 - s;

			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);

			if(oper->operand2.op == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			}else if(oper->operand2.op == OPERAND_REG8){
				*(uint8_t*)oper->operand2.reg = d;
			}

			vm_fprintf(stdout,"neg %s\n", oper->alias_operand2);
			break;
		case 4:	//MUL
			d16 = (uint16_t)(core->reg.ax & 0x00ff) * (uint16_t)s;
			core->reg.ax = d16;

			update_flags(FLAGS_P, d16 % 2 == 0);
			update_flags(FLAGS_Z, d16 == 0);
			update_flags(FLAGS_S, 1);
			if((d16 & 0xff00) == 0){
				update_flags(FLAGS_C, 0);
				update_flags(FLAGS_O, 0);
			} else {
				update_flags(FLAGS_C, 1);
				update_flags(FLAGS_O, 1);
			}

			vm_fprintf(stdout,"mul %s\n", oper->alias_operand2);
			break;
		case 5:	//IMUL
			d16 = (int16_t)(core->reg.ax & 0x00ff) * (int16_t)s;
			core->reg.ax = (uint16_t)d16;

			update_flags(FLAGS_P, d16 % 2 == 0);
			update_flags(FLAGS_Z, d16 == 0);
			//update_flags(FLAGS_S, d16 >> 7);
			if((d16 & 0xff00) == 0 || (d16 & 0xff00) == 0xff00){
				update_flags(FLAGS_C, 0);
				update_flags(FLAGS_O, 0);
			} else {
				update_flags(FLAGS_C, 1);
				update_flags(FLAGS_O, 1);
			}

			vm_fprintf(stdout,"imul %s\n", oper->alias_operand2);
			break;
		case 6:	//DIV
			ax = core->reg.ax;

			al = ax / s;
			ah = ax % s;

			core->reg.ax = (uint16_t)ah << 8 + (uint16_t)al & 0xff00;

			update_flags(FLAGS_Z, al == 0 && ah == 0);

			vm_fprintf(stdout,"div %s\n", oper->alias_operand2);
			break;
		case 7:	//IDIV
			ax = core->reg.ax;

			al = (int8_t)((int16_t)ax / (int16_t)s);
			ah = (int8_t)((int16_t)ax % (int16_t)s);

			core->reg.ax = (uint16_t)((int16_t)ah << 8 + (int16_t)al & 0xff00);

			update_flags(FLAGS_Z, al == 0 && ah == 0);
			update_flags(FLAGS_S, al < 0);

			vm_fprintf(stdout,"idiv %s\n", oper->alias_operand2);
			break;
	}

	return 0;
}

int instruct_process_table_f7(struct operand* oper){
	uint8_t op = 0;
	cpu8086_core_t* core = get_core();

	if(oper->operand1_type != OPERAND_OP){
		return -1;
	}

	op = oper->operand1.op;

	uint16_t s = 0;
	if(oper->operand2.op == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t im;
	uint16_t d;
	uint16_t ax;
	uint8_t al;
	uint8_t ah;
	uint32_t eax;

	uint16_t a;
	uint16_t b;

	switch(op){
		case 0:	//TEST
			im = instruct_read_word();
			d = s & im;

			//更新标志位 CF,PF,ZF,SF,OF
			update_flags(FLAGS_C, 0);
			update_flags(FLAGS_O, 0);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);

			vm_fprintf(stdout,"test %s, %d\n", oper->alias_operand2, im);
			break;
		case 1:	//not used
			break;
		case 2:	//NOT
			d = ~s;

			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);

			if(oper->operand2.op == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			}else if(oper->operand2.op == OPERAND_REG16){
				*oper->operand2.reg = d;
			}

			vm_fprintf(stdout,"not %s\n", oper->alias_operand2);
			break;
		case 3:	//NEG
			d = 0 - s;

			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);

			if(oper->operand2.op == OPERAND_ADDR){
				vm_write_word(oper->operand2.addr, d);
			}else if(oper->operand2.op == OPERAND_REG16){
				*oper->operand2.reg = d;
			}

			vm_fprintf(stdout,"neg %s\n", oper->alias_operand2);
			break;
		case 4:	//MUL
			d = (uint32_t)(core->reg.ax & 0x00ff) * (uint32_t)s;
			core->reg.ax = (uint16_t)d;
			core->reg.dx = (uint16_t)(d>>16);

			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			//update_flags(FLAGS_S, 0);
			if((d & 0xff00) == 0){
				update_flags(FLAGS_C, 0);
				update_flags(FLAGS_O, 0);
			} else {
				update_flags(FLAGS_C, 1);
				update_flags(FLAGS_O, 1);
			}

			vm_fprintf(stdout,"mul %s\n", oper->alias_operand2);
			break;
		case 5:	//IMUL
			d = (int32_t)(core->reg.ax & 0x00ff) * (int32_t)s;
			core->reg.ax = (uint16_t)d;
			core->reg.dx = (uint16_t)(d>>16);

			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			if((d & 0xff00) == 0 || (d & 0xff00) == 0xff00){
				update_flags(FLAGS_C, 0);
				update_flags(FLAGS_O, 0);
			} else {
				update_flags(FLAGS_C, 1);
				update_flags(FLAGS_O, 1);
			}

			vm_fprintf(stdout,"imul %s\n", oper->alias_operand2);
			break;
		case 6:	//DIV
			eax = ((uint32_t)core->reg.dx << 16) + (uint32_t)core->reg.ax;

			a = (uint16_t)(eax / s);
			d = (uint16_t)(eax % s);

			core->reg.ax = a;
			core->reg.dx = d;

			update_flags(FLAGS_Z, eax == 0);

			vm_fprintf(stdout,"div %s\n", oper->alias_operand2);
			break;
		case 7:	//IDIV
			eax = ((int32_t)core->reg.dx << 16) + (int32_t)core->reg.ax;

			a = (uint16_t)(eax / s);
			d = (uint16_t)(eax % s);

			core->reg.ax = a;
			core->reg.dx = d;

			update_flags(FLAGS_Z, eax == 0);
			update_flags(FLAGS_S, eax < 0);

			vm_fprintf(stdout,"idiv %s\n", oper->alias_operand2);
			break;
	}

	return 0;
}

int instruct_process_clc(struct operand* oper){
	cpu8086_core_t* core = get_core();

	update_flags(FLAGS_C, 0);

	vm_fprintf(stdout,"clc\n");
	return 0;
}

int instruct_process_stc(struct operand* oper){
	cpu8086_core_t* core = get_core();

	update_flags(FLAGS_C, 1);

	vm_fprintf(stdout,"stc\n");
	return 0;
}

int instruct_process_cli(struct operand* oper){
	cpu8086_core_t* core = get_core();

	update_flags(FLAGS_I, 0);

	vm_fprintf(stdout,"cli\n");
	return 0;
}

int instruct_process_sti(struct operand* oper){
	cpu8086_core_t* core = get_core();

	update_flags(FLAGS_I, 1);

	vm_fprintf(stdout,"sti\n");
	return 0;
}

int instruct_process_cld(struct operand* oper){
	cpu8086_core_t* core = get_core();

	update_flags(FLAGS_D, 0);

	vm_fprintf(stdout,"cld\n");
	return 0;
}

int instruct_process_std(struct operand* oper){
	cpu8086_core_t* core = get_core();

	update_flags(FLAGS_D, 1);

	vm_fprintf(stdout,"cld\n");
	return 0;
}

int instruct_process_table_fe(struct operand* oper){
	uint8_t op = 0;
	cpu8086_core_t* core = get_core();

	if(oper->operand1_type != OPERAND_OP){
		return -1;
	}

	op = oper->operand1.op;

	uint8_t s = 0;
	if(oper->operand2.op == OPERAND_ADDR){
		s = vm_read_byte(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG8){
		s = *(uint8_t*)oper->operand2.reg;
	} else {
		return -1;
	}

	uint8_t d = 0;

	switch(op){
		case 0:	//INC
			d = s + 1;
			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, s >  d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((s & 0x0f) > (d & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (s & 0x80));

			vm_fprintf(stdout,"inc %s\n", oper->alias_operand2);
			break;
		case 1:	//DEC
			d = s - 1;
			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, s >  d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((s & 0x0f) < (d & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 7);
			update_flags(FLAGS_O, (d & 0x80) ^ (s & 0x80));

			vm_fprintf(stdout,"dec %s\n", oper->alias_operand2);
			break;
	}

	if(oper->operand2.op == OPERAND_ADDR){
		vm_write_byte(oper->operand2.addr, d);
	}else if(oper->operand2.op == OPERAND_REG8){
		*(uint8_t*)oper->operand2.reg = d;
	}

	return 0;
}

int instruct_process_table_ff(struct operand* oper){
	uint8_t op = 0;
	cpu8086_core_t* core = get_core();

	if(oper->operand1_type != OPERAND_OP){
		return -1;
	}

	op = oper->operand1.op;

	uint16_t s = 0;
	if(oper->operand2.op == OPERAND_ADDR){
		s = vm_read_word(oper->operand2.addr);
	}else if(oper->operand2.op == OPERAND_REG16){
		s = *oper->operand2.reg;
	} else {
		return -1;
	}

	uint16_t d;
	switch(op){
		case 0:	//INC
			d = s + 1;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, s >  d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((s & 0x0f) > (d & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (s & 0x8000));

			if(oper->operand2.op == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			}else if(oper->operand2.op == OPERAND_REG8){
				oper->operand2.reg = d;
			}

			vm_fprintf(stdout, "inc %s\n", oper->alias_operand2);
			break;
		case 1:	//DEC
			d = s - 1;

			//更新标志位 CF,PF,AF,ZF,SF,OF
			update_flags(FLAGS_C, s >  d);
			update_flags(FLAGS_P, d % 2 == 0);
			update_flags(FLAGS_A, ((s & 0x0f) < (d & 0x0f)));
			update_flags(FLAGS_Z, d == 0);
			update_flags(FLAGS_S, d >> 15);
			update_flags(FLAGS_O, (d & 0x8000) ^ (s & 0x8000));

			if(oper->operand2.op == OPERAND_ADDR){
				vm_write_byte(oper->operand2.addr, d);
			}else if(oper->operand2.op == OPERAND_REG8){
				oper->operand2.reg = d;
			}

			vm_fprintf(stdout, "dec %s\n", oper->alias_operand2);
			break;
		case 2:	//CALL intra
			break;
		case 3:	//CALL intersegment
			break;
		case 4:	//JMP intra
			break;
		case 5:	//JMP intersegment
			break;
		case 6:	//PUSH
			break;
		case 7:	//not used
			break;
	}

	return 0;
}

static int cpu8086_proc_instruction(void){
	uint8_t opcode = instruct_read_byte();
	int nbyteproc = 0;

	struct operand oper;

	if(cpu8086_instruction_table[opcode].parse)
	  nbyteproc = cpu8086_instruction_table[opcode].parse(&oper);

	if(nbyteproc >= 0 && cpu8086_instruction_table[opcode].proc){
		if(cpu8086_instruction_table[opcode].proc(&oper) < 0){
			vm_fprintf(stderr, "cpu 8086 instruction execute error\n");
			exit(-1);
		}
	}

	return nbyteproc;
}

/*
 * cpu处理主函数
 */
int cpu8086_proc(){
	cpu8086_core_t * core = get_core();

	core->oldip = core->reg.ip;

	return cpu8086_proc_instruction();
}


static addr_t addr_rm_mod0(uint8_t op){
	cpu8086_core_t *core = get_core();

	uint16_t disp;
	addr_t addr = 0;

	switch(op){
		case 0:
			addr = (addr_t)core->reg.ds * 16 + (addr_t)core->reg.bx + (addr_t)core->reg.si;
			break;
		case 1:
			addr = (addr_t)core->reg.ds * 16 + (addr_t)core->reg.bx + (addr_t)core->reg.di;
			break;
		case 2:
			addr = (addr_t)core->reg.ss * 16 + (addr_t)core->reg.bp + (addr_t)core->reg.si;
			break;
		case 3:
			addr = (addr_t)core->reg.ss * 16 + (addr_t)core->reg.bp + (addr_t)core->reg.di;
			break;
		case 4:
			addr = (addr_t)core->reg.ds * 16 + (addr_t)core->reg.si;
			break;
		case 5:
			addr = (addr_t)core->reg.ds * 16 + (addr_t)core->reg.di;
			break;
		case 6:
			//addr = (addr_t)core->reg.ds * 16 + disp;
			disp = instruct_read_word();
			addr = vm_addr_calc(core->reg.ds * 16, disp);
			break;
		case 7:
			addr = (addr_t)core->reg.ds * 16 + (addr_t)core->reg.bx;
			break;                              
	}                                       
	return addr;                            
}                                           

static uint16_t addr_rm_mod0_offset(uint8_t op){
	cpu8086_core_t *core = get_core();

	uint16_t addr = 0;
	uint16_t disp;

	switch(op){
		case 0:
			addr = core->reg.bx + core->reg.si;
			break;
		case 1:
			addr = core->reg.bx + core->reg.di;
			break;
		case 2:
			addr = core->reg.bp + core->reg.si;
			break;
		case 3:
			addr = core->reg.bp + core->reg.di;
			break;
		case 4:
			addr = core->reg.si;
			break;
		case 5:
			addr = core->reg.di;
			break;
		case 6:
			disp = instruct_read_word();
			addr = vm_addr_calc(0, disp);
			break;
		case 7:
			addr = core->reg.bx;
			break;
	}

	return addr;
}

static uint16_t addr_rm_mod1_offset(uint8_t op){
	cpu8086_core_t *core = get_core();

	uint16_t addr = 0;
	uint8_t  disp8 = 0;

	switch(op){
		case 0:
			disp8 = instruct_read_byte();
			addr = core->reg.bx + core->reg.si + disp8;
			break;
		case 1:
			disp8 = instruct_read_byte();
			addr = core->reg.bx + core->reg.di + disp8;
			break;
		case 2:
			disp8 = instruct_read_byte();
			addr = core->reg.bp + core->reg.si + disp8;
			break;
		case 3:
			disp8 = instruct_read_byte();
			addr = core->reg.bp + core->reg.di + disp8;
			break;
		case 4:
			disp8 = instruct_read_byte();
			addr = core->reg.si + disp8;
			break;
		case 5:
			disp8 = instruct_read_byte();
			addr = core->reg.di + disp8;
			break;
		case 6:
			disp8 = instruct_read_byte();
			addr = core->reg.bp + disp8;
			break;
		case 7:
			disp8 = instruct_read_byte();
			addr = core->reg.bx + disp8;
			break;
	}

	return addr;
}

static addr_t addr_rm_mod1(uint8_t op){
	cpu8086_core_t *core = get_core();
	uint8_t disp8 = 0;

	addr_t addr = 0;

	switch(op){
		case 0:
			disp8 = instruct_read_byte();
			addr = (addr_t)core->reg.ds * 16 + core->reg.bx + core->reg.si + disp8;
			break;
		case 1:
			disp8 = instruct_read_byte();
			addr = (addr_t)core->reg.ds * 16 + core->reg.bx + core->reg.di + disp8;
			break;
		case 2:
			disp8 = instruct_read_byte();
			addr = (addr_t)core->reg.ss * 16 + core->reg.bp + core->reg.si + disp8;
			break;
		case 3:
			disp8 = instruct_read_byte();
			addr = (addr_t)core->reg.ss * 16 + core->reg.bp + core->reg.di + disp8;
			break;
		case 4:
			disp8 = instruct_read_byte();
			addr = (addr_t)core->reg.ds * 16 + core->reg.si + disp8;
			break;
		case 5:
			disp8 = instruct_read_byte();
			addr = (addr_t)core->reg.ds * 16 + core->reg.di + disp8;
			break;
		case 6:
			disp8 = instruct_read_byte();
			addr = (addr_t)core->reg.ds * 16 + core->reg.bp + disp8;
			break;
		case 7:
			disp8 = instruct_read_byte();
			addr = (addr_t)core->reg.ds * 16 + core->reg.bx + disp8;
			break;
	}

	return addr;
}

static uint16_t addr_rm_mod2_offset(uint8_t op){
	cpu8086_core_t *core = get_core();

	uint16_t addr = 0;
	uint16_t disp16 = 0;

	switch(op){
		case 0:
			disp16 = instruct_read_word();
			addr = core->reg.bx + core->reg.si + disp16;
			break;
		case 1:
			disp16 = instruct_read_word();
			addr = core->reg.bx + core->reg.di + disp16;
			break;
		case 2:
			disp16 = instruct_read_word();
			addr = core->reg.bp + core->reg.si + disp16;
			break;
		case 3:
			disp16 = instruct_read_word();
			addr = core->reg.bp + core->reg.di + disp16;
			break;
		case 4:
			disp16 = instruct_read_word();
			addr = core->reg.si + disp16;
			break;
		case 5:
			disp16 = instruct_read_word();
			addr = core->reg.di + disp16;
			break;
		case 6:
			disp16 = instruct_read_word();
			addr = core->reg.bp + disp16;
			break;
		case 7:
			disp16 = instruct_read_word();
			addr = core->reg.bx + disp16;
			break;
	}

	return addr;
}

static addr_t addr_rm_mod2(uint8_t op){
	cpu8086_core_t *core = get_core();
	uint16_t disp16 = 0;
	addr_t addr = 0;

	switch(op){
		case 0:
			disp16 = instruct_read_word();
			addr = (addr_t)core->reg.ds * 16 + core->reg.bx + core->reg.si + disp16;
			break;
		case 1:
			disp16 = instruct_read_word();
			addr = (addr_t)core->reg.ds * 16 + core->reg.bx + core->reg.di + disp16;
			break;
		case 2:
			disp16 = instruct_read_word();
			addr = (addr_t)core->reg.ss * 16 + core->reg.bp + core->reg.si + disp16;
			break;
		case 3:
			disp16 = instruct_read_word();
			addr = (addr_t)core->reg.ss * 16 + core->reg.bp + core->reg.di + disp16;
			break;
		case 4:
			disp16 = instruct_read_word();
			addr = (addr_t)core->reg.ds * 16 + core->reg.si + disp16;
			break;
		case 5:
			disp16 = instruct_read_word();
			addr = (addr_t)core->reg.ds * 16 + core->reg.di + disp16;
			break;
		case 6:
			disp16 = instruct_read_word();
			addr = (addr_t)core->reg.ds * 16 + core->reg.bp + disp16;
			break;
		case 7:
			disp16 = instruct_read_word();
			addr = (addr_t)core->reg.ds * 16 + core->reg.bx + disp16;
			break;
	}

	return addr;
}

#define OPERAND_MOD(byte) (((byte) >> 6) & 0x03)
#define OPERAND_OPERAND1(byte) ((byte) >> 3 & 0x07)
#define OPERAND_OPERAND2(byte) ((byte) & 0x07)

int parse_format_seg2rm_16(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint8_t byte = instruct_read_byte();
	uint8_t mod = OPERAND_MOD(byte);
	//REG
	uint8_t op1 = OPERAND_OPERAND1(byte);
	//RM8
	uint8_t op2 = OPERAND_OPERAND2(byte);

	if(op1 & 0x04){
		vm_fprintf(stderr,"not used\n");
		return 0;
	}

	uint16_t* registers[4] = {
		&core->reg.es,
		&core->reg.cs,
		&core->reg.ss,
		&core->reg.ds,
	};

	uint16_t* registers2[] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	oper->noperand = 2;
	oper->operand1_type = OPERAND_SEGMENT;
	oper->operand1.reg = registers[op1];

	strncpy(oper->alias_operand1, register_alias_sement[op1], 32);

	switch(mod){
		case 0:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod0(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 1:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod1(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 2:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod2(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 3:
			oper->operand2_type = OPERAND_REG16;
			oper->operand2.reg = registers2[op2];

			strncpy(oper->alias_operand2, register_alias_mod11[op2].name_16, 32);

			break;
	}

	return 0;
}

int parse_format_reg2rm_8(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint8_t byte = instruct_read_byte();
	uint8_t mod = OPERAND_MOD(byte);
	//REG
	uint8_t op1 = OPERAND_OPERAND1(byte);
	//RM8
	uint8_t op2 = OPERAND_OPERAND2(byte);

	uint8_t* registers[8] = {
		(uint8_t*)&core->reg.ax,
		(uint8_t*)&core->reg.cx,
		(uint8_t*)&core->reg.dx,
		(uint8_t*)&core->reg.bx,
		(uint8_t*)&core->reg.ax + 1,
		(uint8_t*)&core->reg.cx + 1,
		(uint8_t*)&core->reg.dx + 1,
		(uint8_t*)&core->reg.bx + 1,
	};

	uint16_t* registers2[] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	oper->noperand = 2;
	oper->operand1_type = OPERAND_REG8;
	oper->operand1.reg = (uint16_t*)registers[op1];

	strncpy(oper->alias_operand1, register_alias_mod00[op1].name, 32);

	switch(mod){
		case 0:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod0(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 1:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod1(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 2:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod2(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);
			break;
		case 3:
			oper->operand2_type = OPERAND_REG8;
			oper->operand2.reg = (uint16_t*)registers2[op2];

			strncpy(oper->alias_operand2, register_alias_mod11[op2].name_8, 32);
			break;
	}

	return 0;
}

int parse_format_reg2rm_lea(struct operand* oper){
	cpu8086_core_t * core = get_core();

	uint8_t byte = instruct_read_byte();
	uint8_t mod = OPERAND_MOD(byte);
	//REG
	uint8_t op1 = OPERAND_OPERAND1(byte);
	//RM8
	uint8_t op2 = OPERAND_OPERAND2(byte);

	uint16_t* registers[8] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	uint16_t* registers2[] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	oper->noperand = 2;
	oper->operand1_type = OPERAND_REG16;
	oper->operand1.reg = registers[op1];

	strncpy(oper->alias_operand1, register_alias_mod00[op1].name, 32);

	switch(mod){
		case 0:
			oper->operand2_type = OPERAND_IM16;
			oper->operand2.im = addr_rm_mod0_offset(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.im);

			break;
		case 1:
			oper->operand2_type = OPERAND_IM16;
			oper->operand2.im = addr_rm_mod1_offset(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.im);

			break;
		case 2:
			oper->operand2_type = OPERAND_IM16;
			oper->operand2.im = addr_rm_mod2_offset(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.im);

			break;
		case 3:
			oper->operand2_type = OPERAND_REG16;
			oper->operand2.reg = registers2[op2];

			strncpy(oper->alias_operand2, register_alias_mod11[op2].name_16, 32);

			break;
	}

	return 0;
}

int parse_format_reg2rm_16(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint8_t byte = instruct_read_byte();
	uint8_t mod = OPERAND_MOD(byte);
	//REG
	uint8_t op1 = OPERAND_OPERAND1(byte);
	//RM8
	uint8_t op2 = OPERAND_OPERAND2(byte);

	uint16_t* registers[8] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	uint16_t* registers2[] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	oper->noperand = 2;
	oper->operand1_type = OPERAND_REG16;
	oper->operand1.reg = registers[op1];

	strncpy(oper->alias_operand1, register_alias_mod00[op1].name, 32);

	switch(mod){
		case 0:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod0(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 1:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod1(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 2:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod2(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 3:
			oper->operand2_type = OPERAND_REG16;
			oper->operand2.reg = registers2[op2];

			strncpy(oper->alias_operand2, register_alias_mod11[op2].name_16, 32);

			break;
	}

	return 0;
}

int parse_format_imm_8(struct operand* oper){
	uint8_t byte = instruct_read_byte();

	oper->noperand = 1;
	oper->operand1_type = OPERAND_IM8;
	oper->operand1.im   = byte;

	snprintf(oper->alias_operand1, 32, "0x%02x", oper->operand1.im);

	return 0;
}

int parse_format_imm_16(struct operand* oper){
	uint16_t word = instruct_read_word();

	oper->noperand = 1;
	oper->operand1_type = OPERAND_IM16;
	oper->operand1.im   = word;

	snprintf(oper->alias_operand1, 32, "0x%04x", oper->operand1.im);

	return 0;
}

int parse_format_ipinc_8(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint8_t byte = instruct_read_byte();

	oper->noperand = 1;
	oper->operand1_type = OPERAND_IP8;
	oper->operand1.offset = (uint8_t)((uint8_t)core->reg.ip + byte); 

	snprintf(oper->alias_operand1, 32, "0x%02x", oper->operand1.offset);

	return 0;
}

int parse_format_ipinc_16(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint16_t word = instruct_read_word();

	oper->noperand = 1;
	oper->operand1_type = OPERAND_IP16;
	oper->operand1.offset = (uint16_t)(core->reg.ip + word);

	snprintf(oper->alias_operand1, 32, "0x%04x", oper->operand1.offset);

	return 0;
}

int parse_format_rm2imm_8(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint8_t byte = instruct_read_byte();
	uint8_t mod = OPERAND_MOD(byte);
	//REG
	uint8_t op1 = OPERAND_OPERAND1(byte);
	//RM8
	uint8_t op2 = OPERAND_OPERAND2(byte);

	uint8_t* registers[8] = {
		(uint8_t*)&core->reg.ax,
		(uint8_t*)&core->reg.cx,
		(uint8_t*)&core->reg.dx,
		(uint8_t*)&core->reg.bx,
		(uint8_t*)&core->reg.ax + 1,
		(uint8_t*)&core->reg.cx + 1,
		(uint8_t*)&core->reg.dx + 1,
		(uint8_t*)&core->reg.bx + 1,
	};

	uint16_t* registers2[] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	oper->noperand = 3;
	oper->operand1_type = OPERAND_OP;
	oper->operand1.op = op1;

	switch(mod){
		case 0:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod0(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 1:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod1(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 2:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod2(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 3:
			oper->operand2_type = OPERAND_REG8;
			oper->operand2.reg = (uint16_t*)registers2[op2];

			strncpy(oper->alias_operand2, register_alias_mod11[op2].name_8, 32);

			break;
	}

	oper->operand3_type = OPERAND_IM8;
	oper->operand3.im = instruct_read_byte();

	snprintf(oper->alias_operand3, 32, "0x%02x", oper->operand3.im);

	return 0;
}

int parse_format_rm2imm_16(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint8_t byte = instruct_read_byte();
	uint8_t mod = OPERAND_MOD(byte);
	//REG
	uint8_t op1 = OPERAND_OPERAND1(byte);
	//RM8
	uint8_t op2 = OPERAND_OPERAND2(byte);

	uint16_t* registers[8] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	uint16_t* registers2[] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	oper->noperand = 3;

	oper->operand1_type = OPERAND_OP;
	oper->operand1.op = op1;

	switch(mod){
		case 0:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod0(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 1:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod1(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 2:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod2(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 3:
			oper->operand2_type = OPERAND_REG16;
			oper->operand2.reg = registers2[op2];

			strncpy(oper->alias_operand2, register_alias_mod11[op2].name_16, 32);

			break;
	}

	oper->operand3_type = OPERAND_IM16;
	oper->operand3.im = instruct_read_word();

	snprintf(oper->alias_operand3, 32, "0x%04x", oper->operand3.im);

	return 0;
}

int parse_format_call_16(struct operand* oper){
	oper->noperand = 2;

	oper->operand1_type = OPERAND_IP16;
	oper->operand1.offset = instruct_read_word();

	snprintf(oper->alias_operand1, 32, "0x%04x", oper->operand1.offset);

	oper->operand2_type = OPERAND_SEGMENT;
	oper->operand2.segment = instruct_read_word();

	snprintf(oper->alias_operand2, 32, "0x%04x", oper->operand1.offset);

	return 0;
}

int parse_format_table_rm_8(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint8_t byte = instruct_read_byte();
	uint8_t mod = OPERAND_MOD(byte);
	//REG
	uint8_t op1 = OPERAND_OPERAND1(byte);
	//RM8
	uint8_t op2 = OPERAND_OPERAND2(byte);

	uint8_t* regs[8] = {
		(uint8_t*)&core->reg.ax,
		(uint8_t*)&core->reg.cx,
		(uint8_t*)&core->reg.dx,
		(uint8_t*)&core->reg.bx,
		(uint8_t*)&core->reg.ax + 1,
		(uint8_t*)&core->reg.cx + 1,
		(uint8_t*)&core->reg.dx + 1,
		(uint8_t*)&core->reg.bx + 1,
	};

	uint16_t* registers2[] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	oper->noperand = 2;

	oper->operand1_type = OPERAND_OP;
	oper->operand1.op = op1;

	switch(mod){
		case 0:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod0(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 1:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod1(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 2:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod2(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 3:
			oper->operand2_type = OPERAND_REG8;
			oper->operand2.reg = (uint16_t*)registers2[op2];

			strncpy(oper->alias_operand2, register_alias_mod11[op2].name_8, 32);

			break;
	}

	return 0;
}

int parse_format_table_rm_16(struct operand* oper){
	cpu8086_core_t * core = get_core();
	uint8_t byte = instruct_read_byte();
	uint8_t mod = OPERAND_MOD(byte);
	//REG
	uint8_t op1 = OPERAND_OPERAND1(byte);
	//RM8
	uint8_t op2 = OPERAND_OPERAND2(byte);

	uint16_t* registers[8] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	uint16_t* registers2[] = {
		&core->reg.ax,
		&core->reg.cx,
		&core->reg.dx,
		&core->reg.bx,
		&core->reg.sp,
		&core->reg.bp,
		&core->reg.si,
		&core->reg.di
	};

	oper->noperand = 2;

	oper->operand1_type = OPERAND_OP;
	oper->operand1.op = op1;

	switch(mod){
		case 0:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod0(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 1:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod1(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 2:
			oper->operand2_type = OPERAND_ADDR;
			oper->operand2.addr = addr_rm_mod2(op2);

			snprintf(oper->alias_operand2, 32, "[0x%04x]", oper->operand2.addr);

			break;
		case 3:
			oper->operand2_type = OPERAND_REG16;
			oper->operand2.reg = registers2[op2];

			strncpy(oper->alias_operand2, register_alias_mod11[op2].name_16, 32);

			break;
	}

	return 0;
}

int cpu8086_init(void){
	cpu8086_core_t * core = get_core();
	assert(core != NULL);

	core->reg.ss = 0;
	core->reg.es = 0;
	core->reg.ds = 0;
	core->reg.cs = 0x7c0;
	core->reg.ip = 0;
	core->reg.ax = 0;
	core->reg.bx = 0;
	core->reg.cx = 0;
	core->reg.dx = 0;
	core->reg.sp = 0;
	core->reg.bp = 0;
	core->reg.si = 0;
	core->reg.di = 0;

	return 1;
}
