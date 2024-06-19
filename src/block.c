#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "my_secmalloc.private.h"
#include "heap.h"
#include "block.h"
 
struct block_mprotect {
  void* address;
  size_t size;
};


void block_set_canary(struct block_entry* block) {
    memcpy(block->address+block->size-sizeof(canary_t),&block->canary,sizeof(canary_t));
}

void block_mprotect(struct block_entry* block) {
    size_t protect_address = (size_t)(block->address) & ~(PAGE_SIZE-1);
    // size_t end_address = (size_t)(block->address + block->size) - ((size_t)(block->address + block->size) % PAGE_SIZE) + PAGE_SIZE;
    // size_t protect_size = end_address - protect_address;
    size_t protect_size = (size_t)(block->address - protect_address) + block->size;
    mprotect((void*)protect_address,protect_size,PROT_READ | PROT_WRITE);
}

/* struct block_neighbors {
    struct block_entry_indexed before;
    struct block_entry_indexed after;
}; */

// This function is O(N), doing better is surely possible.
void block_free(struct block_entry_indexed* block_ref) {
    struct block_entry *block_before = NULL;
    struct block_entry *block_after = NULL;
    struct block_entry* current_block = HEAP.block_vector.start;

    void* block_start = block_ref->block->address;
    void* block_end = block_ref->block->address + block_ref->block->size;
    size_t block_before_index;
    size_t block_after_index;

    for (size_t i=0;i<HEAP.block_vector.size;i++) {
        if (current_block->status == FREE) {
            // printf("[CHECK] %p | (+%zu)%p == %p", current_block->address, current_block->size, current_block->address+current_block->size, block_start);
            if ((current_block->address+current_block->size) == block_start) {
                block_before = current_block;
                block_before_index = i;
            }
            else if (current_block->address == block_end) {
                block_after = current_block;
                block_after_index = i;
            }
        }
        current_block++;
    }

    // printf("Block=%p Before=%p After=%p\n", block_ref->block->address, block_before, block_after);
    if (block_before != NULL && block_after != NULL) {
        block_before->size += (block_ref->block->size + block_after->size);
        vector_erase(&HEAP.block_vector,block_ref->index);
        vector_erase(&HEAP.block_vector,block_after_index);
    } else if (block_before != NULL) {
        block_before->size += block_ref->block->size;
        vector_erase(&HEAP.block_vector,block_ref->index);
    } else if (block_after != NULL) {
        block_ref->block->size += block_after->size;
        vector_erase(&HEAP.block_vector,block_after_index);
    } else {
        block_ref->block->status = FREE;
    }

    // Should i put all the following logic into a dedicated function ?
    // Can i make the following code better ? (in both readbility and simplicity)
    struct block_entry* first_block = block_before != NULL ?
        block_before : block_ref->block;

    void* clean_addr = first_block->address;
    // Using the and operand assumes that PAGE_SIZE is always a power of 2.
    if (((size_t)first_block->address & (PAGE_SIZE-1)) > 0) {
        clean_addr += PAGE_SIZE - ((size_t)first_block->address % PAGE_SIZE);
    }
    size_t clean_size = (first_block->address + first_block->size) - clean_addr;
    clean_size -= clean_size % PAGE_SIZE;

    mprotect(clean_addr,clean_size,PROT_NONE); // THIS MUST MEMORY ALIGNED !
    // printf("[ALIGNEMENT] mprotect(%p, %lx)\n", clean_addr, clean_size);
    madvise(clean_addr,clean_size,MADV_DONTNEED);

    if (first_block->address+first_block->size == HEAP.end) {
        HEAP.end = first_block->address;
        vector_erase(&HEAP.block_vector,block_before == NULL ? block_ref->index : block_before_index);
    }

    HEAP.nbusyblocks--;
}
