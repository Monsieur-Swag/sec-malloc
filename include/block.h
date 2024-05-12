#ifndef _BLOCK_H
#define _BLOCK_H

struct block_mprotect {
  void* address;
  size_t size;
};

void block_set_canary(struct block_entry* block);
void block_mprotect(struct block_entry* block);
void block_free(struct block_entry_indexed* block_ref);

#endif