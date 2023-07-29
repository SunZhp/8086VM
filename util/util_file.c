#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.h"
#include "util/util_file.h"

struct vm_file_handle* vm_file_handle_create(void){
	struct vm_file_handle* handle = (struct vm_file_handle*)malloc(sizeof(struct vm_file_handle));

	assert(handle != NULL);
	
	handle->fd = open(g_config.hdpath, O_RDWR);
	if(handle->fd <= 0){
		free(handle);
		return NULL;
	}

	return handle;
}

int vm_file_handle_read(struct vm_file_handle* handle, uint8_t* buf, uint32_t size){
	assert(handle);
	assert(buf);
	assert(handle->fd > 0);
	assert(size > 0);

	return read(handle->fd, buf, size);
}

int vm_file_handle_write(struct vm_file_handle* handle, uint8_t* buf, uint32_t size){
	assert(handle);
	assert(buf);
	assert(handle->fd > 0);
	assert(size > 0);

	return write(handle->fd, buf, size);
}

void vm_file_handle_destroy(struct vm_file_handle* handle){
	assert(handle);
	assert(handle->fd > 0);

	close(handle->fd);
	free(handle);
}

int vm_file_handle_seek(struct vm_file_handle* handle, uint64_t seek){
	assert(handle);
	assert(handle->fd > 0);

	return lseek(handle->fd, seek, SEEK_SET);
}
