#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#include "vector.h"

int32_t vector_init(struct vector* vector, size_t capacity, size_t item_size) {
  if (capacity == 0 || item_size == 0)
    return -1;

  vector->item_size = item_size;
  vector->size = 0;
  vector->capacity = capacity;

  void* vector_start = mmap(NULL,V_SIZE,PROT_READ | PROT_WRITE,MAP_ANON | MAP_PRIVATE,-1,0);

  if (vector_start == MAP_FAILED)
    return -1;

  vector->start = vector_start;
  return 0;
}

static int32_t vector_shrink_to_fit(struct vector* vector) {
  vector->capacity = vector->size;
  void* vector_start = mremap(
    vector->start,
    V_SIZE,vector->size*vector->item_size,0
  ); // Should i also use MREMAP_MAYMOVE when shrinking the allocation ?
  if (vector_start == MAP_FAILED) // Can this even fail ?
    return -1;

  vector->start = vector_start;
  return 0;
}

ssize_t vector_push(struct vector* vector, void* item) {
  void* vector_start = vector->start;
  ssize_t index;

  if (vector->size == vector->capacity) {
    vector_start = mremap(
      vector_start,
      V_SIZE,V_SIZE*2,
      MREMAP_MAYMOVE
    );
    if (vector_start == MAP_FAILED)
      return -1;
    vector->capacity = vector->capacity*2;
    vector->start = vector_start;
  }

  index = vector->size;
  memcpy(V_INDEX(index),item,vector->item_size);
  vector->size++;
  return index;
}

int32_t vector_erase(struct vector* vector, size_t index) {
  if (vector->size == 0 || index > vector->size + 1)
    return -1;

  if (index+1 != vector->size)
    memcpy(V_INDEX(index),V_INDEX(vector->size-1),vector->item_size);
  vector->size--;

  if (V_SIZE >= PAGE_SIZE && (vector->capacity >> 1 >= vector->size))
    return vector_shrink_to_fit(vector);
  return 0;
}

int32_t vector_update_capacity(struct vector* vector, size_t capacity) {
  if (capacity < vector->size)
    return -1;

  void* vector_start = mremap(
    vector->start,
    V_SIZE,capacity*vector->item_size,
    MREMAP_MAYMOVE
  );
  if (vector_start == MAP_FAILED)
    return -1;

  vector->capacity = capacity;
  vector->start = vector_start;
  return 0;
}

int32_t vector_clear(struct vector* vector) {
  int32_t status = munmap(vector->start,V_SIZE);
  if (status >= 0) {
    vector->size = 0;
    vector->capacity = 0;
  }
  return status;
}
