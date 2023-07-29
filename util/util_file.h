#ifndef VM_UTIL_FILE_H
#define VM_UTIL_FILE_H

#include <stdint.h>

struct vm_file_handle{
	int fd;
};

struct vm_file_handle* vm_file_handle_create(void);

int vm_file_handle_seek(struct vm_file_handle* handle, uint64_t seek);
int vm_file_handle_read(struct vm_file_handle* handle,uint8_t* buffer, uint32_t size);
int vm_file_handle_write(struct vm_file_handle* handle,uint8_t* buffer, uint32_t size);

void vm_file_handle_destroy(struct vm_file_handle* handle);

#endif
