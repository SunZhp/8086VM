#ifndef VM_VGIO_H
#define VM_VGIO_H

#define VGIO_WIDTH 	80
#define VGIO_HEGIHT 25

#define VGIO_COMMAND_UNKNOWN       (-1)
#define VGIO_COMMAND_SET_CURSOR 	0
//#define VGIO_COMMAND_GETCURSOR 	1
//#define VGIO_COMMAND_SHOW	   	2
//#define VGIO_COMMAND_SETANDSHOW 3
#define VGIO_COMMAND_SET_FG     4  //设置前景色
#define VGIO_COMMAND_SET_BG     5  //设置背景色
#define VGIO_COMMAND_SET_CHAR   6
#define VGIO_COMMAND_GET_CHAR   7



#define SHM_SCREEN_KEY 0x13141516
#define VGIO_UNIX_SOCKET "/var/run/.vgio_unix.sock"

//命令交互格式
struct vgio_command{
	int command;
	struct {
		int x;
		int y;
		int bgcolor;
		int fgcolor;
		char c;
	} pixel;
};

//初始化vgio环境，创建vgio输出窗口
void vgio_init(void);

//保存光标位置
void vgio_cursor_push(void);
//恢复光标位置
void vgio_cursor_pop(void);

//设置光标位置
void vgio_cursor_set(int x, int y);
//设置背景色 40-47
void vgio_cursor_bkcolor(int color);
//设置前景色 30-37
void vgio_cursor_fgcolor(int color);

#endif
