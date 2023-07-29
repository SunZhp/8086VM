#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "8086/pci.h"

#define SHM_PIC (sizeof(struct pci_record) * 0x10000)

struct pci_record * g_pci_port = NULL;

int pci_init(void){
	g_pci_port = malloc(SHM_PIC);
	if(g_pci_port == NULL){
		return 0;
	}

	memset(g_pci_port, 0, SHM_PIC);

	return 1;
}

uint8_t pci_in_byte(uint16_t port){
	if(g_pci_port[port].pci_func_in_8){
		g_pci_port[port].pci_func_in_8((uint8_t*)&((struct pci_record*)g_pci_port)[port].v);
	}

	return (uint8_t)((struct pci_record*)g_pci_port)[port].v;
}

uint16_t pci_in_word(uint16_t port){
	if(g_pci_port[port].pci_func_in_16){
		g_pci_port[port].pci_func_in_16(&((struct pci_record*)g_pci_port)[port].v);
	}

	return ((struct pci_record*)g_pci_port)[port].v;
}

void pci_out_byte(uint16_t port, uint8_t byte){
	*(uint8_t*)&((struct pci_record*)g_pci_port)[port].v = byte;

	if(g_pci_port[port].pci_func_out_8){
		g_pci_port[port].pci_func_out_8(port, byte);
	}
}

void pci_out_word(uint16_t port, uint16_t word){
	((struct pci_record*)g_pci_port)[port].v = word;

	if(g_pci_port[port].pci_func_out_16){
		g_pci_port[port].pci_func_out_16(port, word);
	}
}

void pci_register_out_8(uint16_t port, pci_func_out_8_t fun){
	g_pci_port[port].pci_func_out_8 = fun;
}

void pci_register_out_16(uint16_t port, pci_func_out_16_t fun){
	g_pci_port[port].pci_func_out_16 = fun;
}

void pci_register_in_8(uint16_t port, pci_func_in_8_t fun){
	g_pci_port[port].pci_func_in_8 = fun;
}

void pci_register_in_16(uint16_t port, pci_func_in_16_t fun){
	g_pci_port[port].pci_func_in_16 = fun;
}

void pci_setvalue_8(uint16_t port, uint8_t val){
	g_pci_port[port].v = (uint16_t)val;
}

void pci_setvalue_16(uint16_t port, uint16_t val){
	g_pci_port[port].v = val;
}
