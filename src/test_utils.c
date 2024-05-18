#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vector.h"
#include "heap.h"
#include "block.h"

void show_maps() {
  char cmd[1024];
  memset(cmd,'\0',1024);
  snprintf(cmd, 1024, "cat /proc/%d/maps", getpid());
  system(cmd);
}

void print_heap() {
    printf("----- [HEAP STATE] -----\n");
    printf("- start=%p end=%p size=%lx\n", HEAP.start, HEAP.end, (HEAP.end - HEAP.start));
    struct block_entry* block = HEAP.block_vector.start;
    for (size_t i=0;i<HEAP.block_vector.size;i++) {
        printf("[BLOCK %zu] addr=%p size=%zu, status=%d\n", i, block->address, block->size, block->status);
        block++;
    }
    printf("----- [HEAP STATE] -----\n");
}

void vector_print(struct vector* vector) {
  printf("[Vector] start=%p capacity=%zu size=%zu item_size=%zu memory_size=%zu\n", vector->start, vector->capacity, vector->size, vector->item_size, V_SIZE);
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

