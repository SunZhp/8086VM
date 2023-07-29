#include <stdarg.h>
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

#ifdef DEBUG
	#define LOG  printf
#else
	#define LOG
#endif

//屏幕显示
char g_vgio_screen[VGIO_HEGIHT][VGIO_WIDTH] = {0};

extern void vgio_cursor_set(int x, int y);

static void vgio_flush(void){
	int i = 0, j = 0;

	vgio_cursor_set(0, 0);

	for ( ; i < VGIO_HEGIHT; i++){
		for(j = 0; j < VGIO_WIDTH; j++){
			printf("%c",g_vgio_screen[i][j]);
		}
		printf("\n");
	}
}

void * thread_flush_screen(void * arg){
	while(1){
		vgio_flush();
		usleep(500000);
	}

	return NULL;
}

static void command_proc(int sockfd, struct vgio_command * vc){
	switch(vc->command){
	case VGIO_COMMAND_SET_CURSOR:
		LOG("command: set-cursor, x %d, y %d\n",vc->pixel.x, vc->pixel.y);
		vgio_cursor_set(vc->pixel.y, vc->pixel.x);
		break;
	case VGIO_COMMAND_GET_CHAR:
		LOG("command: get-char, x %d, y %d\n",vc->pixel.x, vc->pixel.y);
		vgio_cursor_set(vc->pixel.y, vc->pixel.x);
		write(sockfd, &g_vgio_screen[vc->pixel.y][vc->pixel.x], 1);
		break;
	case VGIO_COMMAND_SET_FG:
		LOG("command: set-fg, fd %d\n",vc->pixel.fgcolor);
		vgio_cursor_fgcolor(vc->pixel.fgcolor);
		break;
	case VGIO_COMMAND_SET_BG:
		LOG("command: set-bg, fd %d\n",vc->pixel.bgcolor);
		vgio_cursor_bkcolor(vc->pixel.bgcolor);
		break;
	case VGIO_COMMAND_SET_CHAR:
		LOG("command: set-char, x %d, y %d, c %c\n",vc->pixel.x, vc->pixel.y, vc->pixel.c);
		g_vgio_screen[vc->pixel.y][vc->pixel.x] = vc->pixel.c;
		break;
	default:
		break;
	}
}



int main(){
	int fd = 0;
	pthread_t pid = 0;

	vgio_init();

	unlink(VGIO_UNIX_SOCKET);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0){
		perror("can not create unix domain socket ");
		return -1;
	}

	struct sockaddr_un s_addr;
	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sun_family = AF_UNIX;
	strncpy(s_addr.sun_path, VGIO_UNIX_SOCKET, strlen(VGIO_UNIX_SOCKET));

	bind(fd, (struct sockaddr*)&s_addr, sizeof(s_addr));

	if(listen(fd, 5) < 0){
		perror("unix domain socket listen failed ");
		return -1;
	}

	int c_sock = 0;
	struct sockaddr_un c_addr;
	int c_len = sizeof(c_addr);

	struct vgio_command vc = {0};

	if(pthread_create(&pid, NULL, thread_flush_screen, NULL) != 0){
		fprintf(stderr, "thread create failed");
		return -1;
	}

	c_sock = accept(fd, (struct sockaddr*)&c_addr, &c_len);

	while(1){
		/*reset command*/
		vc.command = VGIO_COMMAND_UNKNOWN;
	
		/*获取数据*/
		if(read(c_sock, &vc, sizeof(vc)) < 0){
			perror("read data failed ");
			break;
		}

		command_proc(c_sock, &vc);
		//vgio_flush();
	}

	pthread_cancel(pid);
	close(fd);
	return 0;
}
