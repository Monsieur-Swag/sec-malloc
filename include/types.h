#ifndef _TYPES_H
#define _TYPES_H

typedef int fd_t;
typedef size_t canary_t; // It's possible to batch canary generation, for example when my block vector goes from 4096 to 8192 i will memset everything to 0 from vector[4096] to vector[8192-1], then it will read (4096*sizeof(canary_t)) bytes from RANDOM_DEVICE to generate 4096 canaries and i will set these canaries from each new struct block_entry that has been created with the mremap reallocation.
// The canary will be constant per block even when its status is changing.
// Maybe i will change this behavior later ot make the security tougher.

#endif