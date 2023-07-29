#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <termios.h>  
#include <assert.h>
#include "config.h"
#include "keyboard.h"

struct key{
	uint8_t ascii;
	uint8_t scancode_single;
} g_key_table[256] = {
	[37] = {37, KEY_S_LEFT},
	[38] = {38, KEY_S_UP},
	[39] = {39, KEY_S_RIGHT},
	[40] = {40, KEY_S_DOWN},
	[48] = {'0', KEY_S_0},
	[49] = {'1', KEY_S_1},
	[50] = {'2', KEY_S_2},
	[51] = {'3', KEY_S_3},
	[52] = {'4', KEY_S_4},
	[53] = {'5', KEY_S_5},
	[54] = {'6', KEY_S_6},
	[55] = {'7', KEY_S_7},
	[56] = {'8', KEY_S_8},
	[57] = {'9', KEY_S_9},
	[65] = {'A', KEY_S_A},
	[66] = {'B', KEY_S_B},
	[67] = {'C', KEY_S_C},
	[68] = {'D', KEY_S_D},
	[69] = {'E', KEY_S_E},
	[70] = {'F', KEY_S_F},
	[71] = {'G', KEY_S_G},
	[72] = {'H', KEY_S_H},
	[73] = {'I', KEY_S_I},
	[74] = {'J', KEY_S_J},
	[75] = {'K', KEY_S_K},
	[76] = {'L', KEY_S_L},
	[77] = {'M', KEY_S_M},
	[78] = {'N', KEY_S_N},
	[79] = {'O', KEY_S_O},
	[80] = {'P', KEY_S_P},
	[81] = {'Q', KEY_S_Q},
	[82] = {'R', KEY_S_R},
	[83] = {'S', KEY_S_S},
	[84] = {'T', KEY_S_T},
	[85] = {'U', KEY_S_U},
	[86] = {'v', KEY_S_V},
	[87] = {'W', KEY_S_W},
	[88] = {'X', KEY_S_X},
	[89] = {'Y', KEY_S_Y},
	[90] = {'Z', KEY_S_Z},
};

#define KEYPOLL_INIT_TOP 256

//环形队列
struct key_poll{
	uint32_t head;
	uint32_t top;
	uint8_t *ascii;
} g_keypoll = {0, 0, NULL};

static int keypoll_empty(void){
	return g_keypoll.top == g_keypoll.head;
}

static int keypoll_full(void){
	return ((g_keypoll.top + 1) % KEYPOLL_INIT_TOP) == g_keypoll.head;
}

static void keypoll_push(uint8_t key){
	if(keypoll_full() == 0){
		g_keypoll.top = (g_keypoll.top + 1) % KEYPOLL_INIT_TOP;
		g_keypoll.ascii[g_keypoll.top] = key;
	}
}

static uint8_t keypoll_pop(void){
	uint8_t r = 0;
	if(keypoll_empty() == 0){
		g_keypoll.head = (g_keypoll.head + 1) % KEYPOLL_INIT_TOP;
		r = g_keypoll.ascii[g_keypoll.head];
	}

	return r;
}

int getch(void){   
	int c=0;   
	struct termios org_opts, new_opts;   
	int res=0;   
	//-----  store old settings -----------   
	res=tcgetattr(STDIN_FILENO, &org_opts);   
	assert(res==0);   
	//---- set new terminal parms --------   
	memcpy(&new_opts, &org_opts, sizeof(new_opts));   
	new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);   
	tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);   
	c=getchar();   
	//------  restore old settings ---------   
	res=tcsetattr(STDIN_FILENO, TCSANOW, &org_opts);assert(res==0);   
	return c;   
}  

void* thread_keyboard_receive(void* arg){
	uint8_t ch = 0;
	while(ch = getch()){
		keypoll_push(ch);
	}
}

int keyboard_init(void){
	g_keypoll.head = 0;
	g_keypoll.top = 0;
	g_keypoll.ascii = (uint8_t*)malloc(KEYPOLL_INIT_TOP);

	if(g_keypoll.ascii == NULL){
		return 0;
	}

	pthread_t tid;
	pthread_create(&tid, NULL, thread_keyboard_receive, NULL);

	if(tid < 0){
		return 0;
	}

	return 1;
}

uint16_t keyboard_read(void){
	uint8_t r = keypoll_pop();
	if(r == 0){
		return 0;
	}

	if(g_key_table[r].ascii == 0){
		return 0;
	}

	uint16_t c = 0;

	c |= (uint16_t)g_key_table[r].ascii;
	c |= (uint16_t)g_key_table[r].scancode_single << 8;

	return c;
}
