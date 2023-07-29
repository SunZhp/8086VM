#include <stdarg.h>
#include <stdio.h>
#include "config.h"
#include "vgio.h"

#if 0
void vm_fprintf(FILE* fp, char* fmt, ...){
	int ret = 0;
	va_list args;

	//int fd = io->_fileno;

	vgio_cursor_push();
	
	va_start(args,fmt);
	ret = fprintf(fp, fmt, args);
	va_end(args);

	vgio_cursor_pop();
}
#endif
