#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include "types.h"
#include "heap.h"
#include "block.h"
#include "utils.h"

#define LOGGING_ENV_VARIABLE "MSM_OUTPUT"
#define TOTAL_SYSTEM_RAM ((size_t)(sysconf(_SC_PHYS_PAGES)*sysconf(_SC_PAGE_SIZE)))
#define AVAILABLE_SYSTEME_RAM ((size_t)(sysconf(_SC_AVPHYS_PAGES)*sysconf(_SC_PAGE_SIZE)))
#define HEAP_MAX_FREE_SPACE (AVAILABLE_SYSTEME_RAM - (HEAP.end - HEAP.start))
#define CANARY_CHECK_DELAY 1000000 // In microseconds

struct heap HEAP = {
    .nbusyblocks = 0,
    .canary_thread = 0 // Is this usefull ?
};


void* heap_thread_function() {
    struct block_entry* block;
    canary_t canary;
    size_t i;
    while (true) {
        block = HEAP.block_vector.start;
        for (i=0;i<HEAP.block_vector.size;i++) {
            if (block->status == PROCESSED)
                continue;
            canary = *(canary_t*)(block->address + block->size - sizeof(canary_t));
            if (canary != block->canary) {
                my_printf("Heap overflow detected at the end of block %p of size %zu", block->address, block->size - sizeof(canary_t));
            }
            block++;
        }
        usleep(CANARY_CHECK_DELAY);
    }
    return NULL;
}

void heap_thread_init() {
    pthread_create(&HEAP.canary_thread,NULL,heap_thread_function,NULL); // Should i do error handling ?
}

void heap_thread_stop() {
    pthread_cancel(HEAP.canary_thread); // Should i do error handling ?
    pthread_join(HEAP.canary_thread,NULL); // Should i do error handling ?
    HEAP.canary_thread = 0; // Is this usefull ?
}

int32_t heap_init(size_t size) {
    int32_t status;
    void* heap_start;

    HEAP.virtual_capacity = TOTAL_SYSTEM_RAM;
    HEAP.has_logging = getenv(LOGGING_ENV_VARIABLE) != NULL;
    if (size > HEAP_MAX_FREE_SPACE)
        return 0;

    // HEAP.capacity = size > PAGE_SIZE ? size : PAGE_SIZE;
    heap_start = mmap(NULL,HEAP.virtual_capacity,PROT_NONE,MAP_ANON | MAP_PRIVATE,-1,0);
    if (heap_start == MAP_FAILED)
        return 0;
    HEAP.start = heap_start;
    HEAP.end = heap_start; // HEAP.start + size;

    status = vector_init(&HEAP.block_vector,64,sizeof(struct block_entry));
    if (status < 0) return status;

    if (HEAP.has_logging)
        heap_thread_init();
    return 0;
}

struct block_entry* heap_get_free_block(size_t size) {
    // This function is not optimal.
    // Everything in this library is not thread safe (if a thread remove elements from the vector the block_entry may be switched within the vector, so the pointer returned by this function could be pointing to a new unexcpected block entry)
    struct block_entry* block = HEAP.block_vector.start;
    for (size_t i=0;i<HEAP.block_vector.size;i++) {
        if (block->status == FREE && block->size >= size)
            return block;
        block++;
    }
    return NULL;
}

void* heap_create_block_at_end(size_t size) {
    struct block_entry new_block = {
        .address = HEAP.end,
        // I will batch the canary generation and setting later.
        .canary = canary_generate(),
        .size = size,
        .status = PROCESSED
    };
    ssize_t vector_index;
    struct block_entry* new_block_entry;
    if ((vector_index = vector_push(&HEAP.block_vector,&new_block)) < 0)
        return NULL;
    HEAP.end += size;

    block_mprotect(&new_block);
    block_set_canary(&new_block);
    new_block_entry = ((struct block_entry*)(HEAP.block_vector.start) + vector_index);
    new_block_entry->status = BUSY;
    HEAP.nbusyblocks++;
    return new_block.address;
}

void* heap_create_block(size_t size) {
    struct block_entry* block;

    if (size > HEAP_MAX_FREE_SPACE)
        return NULL;

    if ((block = heap_get_free_block(size))) {
        if (block->size == size) {
            block->status = BUSY;
            return block->address;
        }

        size_t remaining_size = block->size - size;
        struct block_entry new_free_block = {
            .address = block->address+size,
            .status = FREE,
            .size = remaining_size
        };
        if (vector_push(&HEAP.block_vector,&new_free_block) < 0) return NULL;

        block->status = PROCESSED; // Is this usefull ?
        block->size = size;
        block_mprotect(block);
        block->status = BUSY;
        HEAP.nbusyblocks++;
        return block->address;
    }

    return heap_create_block_at_end(size);
}

int32_t heap_clear() {
    munmap(HEAP.start,HEAP.virtual_capacity);
    int32_t status = vector_clear(&HEAP.block_vector);
    if (status < 0) return status;

    if (HEAP.has_logging)
        heap_thread_stop();
    HEAP.nbusyblocks = 0;
    return 0;
}

// This function is O(N) but would be O(1) just by using a hashmap.
// I can always implement a hashmap later in the same way i implemented the "vector" structure.
struct block_entry_indexed heap_get_block(void* ptr) {
    struct block_entry_indexed block_ref = {
        .block = NULL,
        .index = 0
    };
    struct block_entry* block = HEAP.block_vector.start;

    if (ptr < HEAP.start || ptr > HEAP.end) return block_ref;
    for (size_t i=0;i<HEAP.block_vector.size;i++) {
        if (block->status == BUSY && block->address == ptr) {
            block_ref.block = block;
            block_ref.index = i;
            return block_ref;
        }
        block++;
    }
    return block_ref;
}
