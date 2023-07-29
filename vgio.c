#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include "vgio.h"
#include "mem.h"
#include "config.h"

#define VGIO_COLOR_FG_BLACK 		30
#define VGIO_COLOR_FG_RED			31
#define VGIO_COLOR_FG_GREEN			32
#define VGIO_COLOR_FG_YELLOW		33
#define VGIO_COLOR_FG_BLUE			34
#define VGIO_COLOR_FG_PURPLE		35
#define VGIO_COLOR_FG_DARKGREEN		36
#define VGIO_COLOR_FG_WHITE			37
#define VGIO_COLOR_BK_BLACK 		40
#define VGIO_COLOR_BK_RED			41
#define VGIO_COLOR_BK_GREEN			42
#define VGIO_COLOR_BK_YELLOW		43
#define VGIO_COLOR_BK_BLUE			44
#define VGIO_COLOR_BK_PURPLE		45
#define VGIO_COLOR_BK_DARKGREEN		46
#define VGIO_COLOR_BK_WHITE			47


//设置光标位置
void vgio_cursor_set(int x, int y);
//设置背景色 40-47
void vgio_cursor_bkcolor(int color);
//设置前景色 30-37
void vgio_cursor_fgcolor(int color);

//保存光标位置
void vgio_cursor_push(void);
//恢复光标位置
void vgio_cursor_pop(void);
//清除屏幕
static void vgio_clear(void);
//刷新页面
static void vgio_flush(void);

void vgio_init(void){
	vgio_clear();
	vgio_cursor_set(0, 0);
	//vgio_cursor_bkcolor(VGIO_COLOR_BK_WHITE);
	//vgio_cursor_fgcolor(VGIO_COLOR_FG_WHITE);
}

static void vgio_clear(void){
	printf("\033[2J");
}

void vgio_cursor_set(int x, int y){
	printf("\033[%d;%dH)", y, x);
}

void vgio_cursor_bkcolor(int color){
	printf("\033[%dm",color);
}

void vgio_cursor_fgcolor(int color){
	printf("\033[%dm",color);
}

void vgio_cursor_push(void){
	printf("\033[s");
}

void vgio_cursor_pop(void){
	printf("\033[u");
}

