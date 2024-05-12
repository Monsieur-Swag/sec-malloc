#ifndef _HEAP_H
#define _HEAP_H

#include <stdint.h>

void* heap_thread_function();
void heap_thread_init();
void heap_thread_stop();
int32_t heap_init(size_t size);
struct block_entry* heap_get_free_block(size_t size);
void* heap_create_block_at_end(size_t size);
void* heap_create_block(size_t size);
int32_t heap_clear();
struct block_entry_indexed heap_get_block(void* ptr); 

#endif

