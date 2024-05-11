#ifndef _SECMALLOC_PRIVATE_H
#define _SECMALLOC_PRIVATE_H

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h> // I think i have to add the pthread library to the compilation flags

#include "my_secmalloc.h"
#include "vector.h"

// sentez vous libre de modifier ce header comme vous le souhaitez

#define PAGE_SIZE ((size_t)sysconf(_SC_PAGE_SIZE)) // I am forced to call sysconf each time i want the system page size because i don't want to use an "extern" value for this :(
#define AVAILABLE_SYSTEME_RAM ((size_t)(sysconf(_SC_AVPHYS_PAGES)*sysconf(_SC_PAGE_SIZE)))
#define TOTAL_SYSTEM_RAM ((size_t)(sysconf(_SC_PHYS_PAGES)*sysconf(_SC_PAGE_SIZE)))
#define RANDOM_DEVICE "/dev/urandom"
#define CANARY_CHECK_DELAY 1000000 // In microseconds
#define LOGGING_ENV_VARIABLE "MSM_OUTPUT"

typedef int fd_t;
typedef size_t canary_t; // It's possible to batch canary generation, for example when my block vector goes from 4096 to 8192 i will memset everything to 0 from vector[4096] to vector[8192-1], then it will read (4096*sizeof(canary_t)) bytes from RANDOM_DEVICE to generate 4096 canaries and i will set these canaries from each new struct block_entry that has been created with the mremap reallocation.
// The canary will be constant per block even when its status is changing.
// Maybe i will change this behavior later ot make the security tougher.

enum block_status { FREE, BUSY, PROCESSED };

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

// The 2 following solutions only make sense for complete heap reallocations using mremap //
// ---------- SOLUTION 1 ---------- //
// The heap growth factor must be calculated as (Growth{n+1} = f(n)*Growth{n}) Where f'(n) < 0 in [0;inf] and f(4096) = 2 and lim n->inf f(n) = 1.
// Something like f(n) = 1 + (1 / log2(n > 12)) | log2(4096) = 12
// We could use a log4/log8/log16 instead to make the logarithm evolution slower.
// If the heap growth factor is not regulated a malloc of 32Gb with an increase of 1 byte would result in an allocation of 64Go to store 32Go+1o which mean we waste 32 Go of RAM for nothing.
// ---------- SOLUTION 2 ---------- //
// We could also use some kind of offset from a certain thresold of capacity (like an offset of 1Mo starting from 4Mo of capacity, or even something like offset = some_function(capacity)).
// The purpose of this offset which is just some extra unused allocated space is to avoid doing mremap calls for little allocations.
// We could make mremap just for allocation bigger than the offset or in some rare cases for a big number of small allocations.
struct heap {
  size_t nbusyblocks;
  void* start;
  void* end;
  // size_t capacity;
  size_t virtual_capacity; // This must be equal to TOTAL_SYSTEM_RAM
  struct vector block_vector;
  struct vector empty_block_vector;
  pthread_t canary_thread;
  bool has_logging;
};

extern struct heap HEAP;
// #define HEAP_FREE_SPACE (HEAP.capacity - (HEAP.end - HEAP.start)) // Should i care about integer overflow there ?
#define HEAP_MAX_FREE_SPACE (AVAILABLE_SYSTEME_RAM - (HEAP.end - HEAP.start))

void *my_malloc(size_t size);
void my_free(void *ptr);
void *my_calloc(size_t nmemb, size_t size);
void *my_realloc(void *ptr, size_t size);

#endif
