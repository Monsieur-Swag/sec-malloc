#ifndef _BLOCK_H
#define _BLOCK_H

void block_set_canary(struct block_entry* block);
void block_mprotect(struct block_entry* block);
void block_free(struct block_entry_indexed* block_ref);

#endif