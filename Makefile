CC=gcc
CFLAGS=-g -O0 -I. -Iarch  -DCPU_8086 -DINTR_8259A -lc -lpthread
LDFLAGS=

SRCLIB=config.o \
	   cpu.o \
	   harddisk.o \
	   keyboard.o \
	   mem.o \
	   vgio.o \
	   vgui.o \
	   util/util_file.o \
	   arch/8086/bios.o \
	   arch/8086/cpu.o  \
	   arch/8086/mem.o  \
	   arch/8086/pci.o

OBJ=vm
VGIO=vgio_cli

.PHONY:clean all

all:$(OBJ) $(VGIO)

$(OBJ): main.c $(SRCLIB)
	$(CC) $(CFLAGS)  -o $@ $(LDFLAGS) $^

$(VGIO):vgio_cli.o $(SRCLIB)
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^

clean:
	@rm -f $(OBJ) $(SRCLIB) $(VGIO)
