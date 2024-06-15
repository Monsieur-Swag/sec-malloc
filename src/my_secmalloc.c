#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdarg.h>
#include <fcntl.h>

#include "types.h"
#include "heap.h"
#include "block.h"
#include "utils.h"

// ----- REMOVE THIS LATER ----- //
#include "test_utils.h"
#define _DEBUG 0
#if _DEBUG == 1
static size_t _DEBUG_VAUE = 0;
#define DEBUG() printf("DEBUG %zu\n", ++_DEBUG_VAUE);
#endif
#define PRINT(...) printf(__VA_ARGS__);putchar('\n');
// ----- REMOVE THIS LATER ----- //

// Reminder: NOTHING in this library is thread safe (i could just put a global mutex stored in the HEAP object)

/*
malloc()
    The  malloc()  function  allocates size bytes and returns a pointer to the allocated memory.  The memory is not initialized.  If
    size is 0, then malloc() returns a unique pointer value that can later be successfully passed to free().  (See "Nonportable  be‐
    havior" for portability issues.)
*/
void *my_malloc(size_t size) {
    // I should return errno for function returning void pointers too (like MAP_FAILED) is MAP_FAILED even an errno ?
    size += sizeof(canary_t); // Should i check for interger overflows ? 0xffffffffffffffff + sizeof(canart_t) would be equal to sizeof(canary_t) - 1 for example.
    if (HEAP.nbusyblocks == 0) {
        if (heap_init(size) >= 0)
            return heap_create_block_at_end(size);
        else
            return NULL;
    }

    return heap_create_block(size);
}

/*
free()
    The  free()  function  frees the memory space pointed to by ptr, which must have been returned by a previous call to malloc() or
    related functions.  Otherwise, or if ptr has already been freed, undefined behavior occurs.  If ptr is  NULL,  no  operation  is
    performed.
*/
void my_free(void *ptr) {
    if (ptr == NULL) return;
    // Is this condition really usefull ?
    // Maybe the heap_get_block(ptr) function would just do the work by returning a NULL reference when the HEAP memory is empty whatsoever.
    if (HEAP.nbusyblocks == 0) {
        if (HEAP.has_logging)
            my_printf("Double free for %p", ptr);
        return; // Should i raise some kind of error when the ptr is not referenced in the heap ?
    }
    
    struct block_entry_indexed block_ref = heap_get_block(ptr);
    if (block_ref.block == NULL) {
        if (HEAP.has_logging)
            my_printf("Double free for %p", ptr); // Should i raise some kind of error when the ptr is not referenced in the heap or just do nothing ?
        return;
    }

    if (HEAP.nbusyblocks == 1) {
        heap_clear();
    } else {
        block_free(&block_ref);
    }
}
/*
I must do exatly as the man page says for all the functions (i didn't even did it for malloc yet)
calloc()
    The  calloc() function allocates memory for an array of nmemb elements of size bytes each and returns a pointer to the allocated
    memory.  The memory is set to zero.  If nmemb or size is 0, then calloc() returns a unique pointer value that can later be  suc‐
    cessfully passed to free().
*/
void  *my_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0)
        return NULL;
    size_t alloc_size = nmemb*size;
    void* addr = my_malloc(alloc_size);
    if (addr != NULL)
        memset(addr,0,alloc_size);
    return addr;
}

/*
realloc()
    The realloc() function changes the size of the memory block pointed to by ptr to size bytes.  The contents of the memory will be
    unchanged in the range from the start of the region up to the minimum of the old and new sizes.  If the new size is larger  than
    the old size, the added memory will not be initialized.

    If ptr is NULL, then the call is equivalent to malloc(size), for all values of size.

    If  size  is  equal  to  zero, and ptr is not NULL, then the call is equivalent to free(ptr) (but see "Nonportable behavior" for
    portability issues).

    Unless ptr is NULL, it must have been returned by an earlier call to malloc or related functions.  If the area  pointed  to  was
    moved, a free(ptr) is done.
*/
void *my_realloc(void *ptr, size_t size)
{
    struct block_entry_indexed block_ref;
    size += sizeof(canary_t);

    if (ptr == NULL)
        return my_malloc(size);

    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    block_ref = heap_get_block(ptr);; // Put this on the top of the function, and do it for any variable declaration in any function in this library.

    if (block_ref.block->size == size)
        return ptr;

    size_t diff = block_ref.block->size - size;
    struct block_entry new_free_block = {
        .canary = canary_generate(),
        .status = FREE,
        .size = diff,
        .address = ptr + size
    }; // If free blocks have a size inferior or equal to 8 they are unusable but could be usefull later after a free block fusion.

    if (size == block_ref.block->size)
        return block_ref.block->address;

    if (size > block_ref.block->size) {
        my_free(ptr); // This is not the best way to do this
        block_free(&block_ref);
        return heap_create_block(size);
    } else {
        block_ref.block->size = size;
        block_set_canary(block_ref.block);
        vector_push(&HEAP.block_vector,&new_free_block);
    }

    return block_ref.block->address;
}

#ifdef DYNAMIC
void *malloc(size_t size)
{
    return my_malloc(size);
}
void free(void *ptr)
{
    my_free(ptr);
}
void *calloc(size_t nmemb, size_t size)
{
    return my_calloc(nmemb, size);
}

void *realloc(void *ptr, size_t size)
{
    return my_realloc(ptr, size);

}

#endif
