#ifndef _TEST_UTILS_H
#define _TEST_UTILS_H

#include "my_secmalloc.private.h"

void show_maps();
void vector_print(struct vector* vector);
int32_t vector_set(struct vector* vector, size_t from, size_t to, void* item);
int32_t vector_make(struct vector* vector, void* items, size_t nmemb);

#endif