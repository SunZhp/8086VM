#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include "vgui.h"
#include "config.h"

int vgui_sock = -1;

struct vgui_cursor{
	uint8_t x;
	uint8_t y;
};

struct vgui_screen {
	struct vgui_cursor cursor;
	int fgcolor;	//前景色
	int bgcolor;	//背景色
} g_vgui_screen_info = {{0,0},0,0};

//打印彩色信息
void print_color(addr_t addr, uint32_t size){
	//XXX
}

//打印黑白信息
void print_bw(addr_t addr, uint32_t size){
	//XXX
}

//打印文本信息
void print_text(addr_t addr, uint32_t size){
}

int vgui_init(void){
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0){
		vm_fprintf(stderr, "create socket error\n");
		return 0;
	}

	struct sockaddr_un c_addr;
	memset(&c_addr, 0, sizeof(c_addr));
	c_addr.sun_family = AF_UNIX;
	strncpy(c_addr.sun_path, VGIO_UNIX_SOCKET, strlen(VGIO_UNIX_SOCKET));
	
	if(connect(fd, (struct sockaddr*)&c_addr, sizeof(c_addr)) < 0){
		perror("connect to unix socket error ");
		vm_fprintf(stderr, "connect failed\n");
		return 0;
	}

	vgui_sock = fd;

	return 1;
}

int vgui_deinit(void){
	close(vgui_sock);

	return 0;
}

//设置光标位置
void vgui_cursor_set(uint8_t x, uint8_t y){
	//维护本地cursor
	g_vgui_screen_info.cursor.x = x;
	g_vgui_screen_info.cursor.y = y;

	struct vgio_command vc;
	vc.command = VGIO_COMMAND_SET_CURSOR;
	vc.pixel.x = x;
	vc.pixel.y = y;

	write(vgui_sock, (void*)&vc, sizeof(struct vgio_command));
}

//获取光标位置
void vgui_cursor_get(uint8_t* x, uint8_t* y){
	*x = g_vgui_screen_info.cursor.x;
	*y = g_vgui_screen_info.cursor.y;
}

//设置背景色 40-47
void vgui_cursor_bkcolor(int color){
	g_vgui_screen_info.bgcolor = color;

	struct vgio_command vc;
	vc.command = VGIO_COMMAND_SET_BG;
	vc.pixel.x = g_vgui_screen_info.cursor.x;
	vc.pixel.y = g_vgui_screen_info.cursor.y;
	vc.pixel.bgcolor = color;

	write(vgui_sock, (void*)&vc, sizeof(struct vgio_command));
}

//设置前景色 30-37
void vgui_cursor_fgcolor(int color){
	g_vgui_screen_info.fgcolor = color;

	struct vgio_command vc;
	vc.command = VGIO_COMMAND_SET_FG;
	vc.pixel.x = g_vgui_screen_info.cursor.x;
	vc.pixel.y = g_vgui_screen_info.cursor.y;
	vc.pixel.fgcolor = color;

	write(vgui_sock, (void*)&vc, sizeof(struct vgio_command));
}

int8_t vgui_char(void){
	struct vgio_command vc;
	vc.command = VGIO_COMMAND_GET_CHAR;
	vc.pixel.x = g_vgui_screen_info.cursor.x;
	vc.pixel.y = g_vgui_screen_info.cursor.y;

	write(vgui_sock, (void*)&vc, sizeof(struct vgio_command));

	uint8_t c = 0;

	read(vgui_sock, (void*)&c, 1);

	return c;
}

void vgui_set_char(uint8_t c){
	struct vgio_command vc;
	vc.command = VGIO_COMMAND_SET_CHAR;
	vc.pixel.x = g_vgui_screen_info.cursor.x;
	vc.pixel.y = g_vgui_screen_info.cursor.y;
	vc.pixel.fgcolor = g_vgui_screen_info.fgcolor;
	vc.pixel.bgcolor = g_vgui_screen_info.bgcolor;
	vc.pixel.c = c;

	if(g_vgui_screen_info.cursor.x + 1 < VGIO_WIDTH){
		g_vgui_screen_info.cursor.x++;
	} else {
		g_vgui_screen_info.cursor.y++;
		g_vgui_screen_info.cursor.x = 0;
	}

	write(vgui_sock, (void*)&vc, sizeof(struct vgio_command));
}
