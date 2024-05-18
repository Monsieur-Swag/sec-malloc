#ifndef _VECTOR_H
#define _VECTOR_H

// #include "heap.h"

#define V_INDEX(index) vector->start + (index) * vector->item_size
#define V_SIZE vector->item_size*vector->capacity

struct vector {
  void* start;
  size_t item_size;
  size_t size;
  size_t capacity;
};

int32_t vector_init(struct vector* vector, size_t capacity, size_t item_size);
ssize_t vector_push(struct vector* vector, void* item);
int32_t vector_erase(struct vector* v, size_t index);
int32_t vector_update_capacity(struct vector* vector, size_t capacity);
int32_t vector_clear(struct vector* vector);

#endif