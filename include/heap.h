#ifndef _HEAP_H
#define _HEAP_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "types.h"
#include "vector.h"



extern struct heap HEAP;

#define PAGE_SIZE ((size_t)sysconf(_SC_PAGE_SIZE)) // I am forced to call sysconf each time i want the system page size because i don't want to use an "extern" value for this :(

struct heap {
  size_t nbusyblocks;
  void* start;
  void* end;
  size_t virtual_capacity;
  struct vector block_vector;
  struct vector empty_block_vector;
  pthread_t canary_thread;
  bool has_logging;
};

enum block_status { 
  FREE,
  BUSY,
  PROCESSED
};

struct block_entry {
  void* address;
  enum block_status status;
  size_t size;
  canary_t canary;
};

struct block_entry_indexed {
  struct block_entry* block;
  size_t index;
};

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

