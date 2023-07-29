#ifndef VM_PIC_8086_H
#define VM_PIC_8086_H

typedef void (*pci_func_out_8_t)(uint16_t, uint8_t);
typedef void (*pci_func_out_16_t)(uint16_t, uint16_t);
typedef void (*pci_func_in_8_t)(uint16_t);
typedef void (*pci_func_in_16_t)(uint16_t);

struct pci_record{
	uint16_t v;			//数据部分
	uint16_t port;
	pci_func_in_8_t pci_func_in_8;
	pci_func_in_16_t pci_func_in_16;
	pci_func_out_8_t pci_func_out_8;
	pci_func_out_16_t pci_func_out_16;
};

int pci_init(void);

//从指定端口读入数据
uint8_t pci_in_byte(uint16_t port);
uint16_t pci_in_word(uint16_t port);

//从写入数据到指定端口
void pci_out_byte(uint16_t port, uint8_t byte);
void pci_out_word(uint16_t port, uint16_t word);

//注册pci
void pci_register_out_8(uint16_t port, pci_func_out_8_t fun);
void pci_register_out_16(uint16_t port, pci_func_out_16_t fun);
void pci_register_in_8(uint16_t port, pci_func_in_8_t fun);
void pci_register_in_16(uint16_t port, pci_func_in_16_t fun);

//设置v
void pci_setvalue_8(uint16_t port, uint8_t val);
void pci_setvalue_16(uint16_t port, uint16_t val);

extern struct pci_record * g_pci_port;

#endif
