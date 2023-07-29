#ifndef VM_MEM_8086_H
#define VM_MEM_8086_H

typedef uint32_t addr_t;

int vm_init(void);

//指定内存读写
uint8_t vm_read_byte(addr_t maddr);
uint16_t vm_read_word(addr_t maddr);
uint32_t vm_read_dword(addr_t maddr);

int vm_read(addr_t maddr, uint8_t* buffer, uint16_t length);

int vm_write_byte(addr_t maddr, uint8_t byte);
int vm_write_word(addr_t maddr, uint16_t word);
int vm_write(addr_t maddr, uint8_t* content, uint16_t length);

//读取指令信息
uint8_t instruct_read_byte();
uint16_t instruct_read_word();

//获取内存大小
uint32_t vm_size(void);
//获取内存地址
void* vm_addr(void);
void* vm_mbr(void);

addr_t vm_addr_calc(uint16_t base, uint16_t offset);

#endif
