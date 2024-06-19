#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vector.h"
#include "heap.h"
#include "block.h"
#include "utils.h"

void show_maps() {
  char cmd[1024];
  memset(cmd,'\0',1024);
  snprintf(cmd, 1024, "cat /proc/%d/maps", getpid());
  system(cmd);
}

void print_heap() {
    my_printf("----- [HEAP STATE] -----");
    my_printf("- start=%p end=%p size=0x%lx", HEAP.start, HEAP.end, (HEAP.end - HEAP.start));
    char buffer[0x1000];
    size_t size_to_copy = 0;
    struct block_entry* block = HEAP.block_vector.start;
    for (size_t i=0;i<HEAP.block_vector.size;i++) {
        my_printf("[BLOCK %zu] addr=%p size=%zu, status=%d", i, block->address, block->size, block->status);
        size_to_copy = block->size;
        if (size_to_copy > 0) {
          if (size_to_copy > 0x1000) size_to_copy = 0x1000;
          memcpy(buffer,block->address,size_to_copy);
          // memset(block->address,0,size_to_copy);
        }
        block++;
    }
    my_printf("----- [HEAP STATE] -----");
}

void vector_print(struct vector* vector) {
  my_printf("[Vector] start=%p capacity=%zu size=%zu item_size=%zu memory_size=%zu", vector->start, vector->capacity, vector->size, vector->item_size, V_SIZE);
}

int32_t vector_set(struct vector* vector, size_t from, size_t to, void* item) {
  if (from+1 > vector->capacity && to+1 > vector->capacity)
    return -1;

  for (size_t i=from;i<to;i++) {
    // The number of call to memcpy is O(N) but it could be O(Log(N)) with a better code.
    memcpy(V_INDEX(i),item,vector->item_size);
  }
  return 0;
}

int32_t vector_make(struct vector* vector, void* items, size_t nmemb) {
  if (vector_update_capacity(vector, nmemb) < 0)
    return -1;

  memcpy(vector->start,items,nmemb*vector->item_size);
  vector->size = nmemb;
  return 0;
}

