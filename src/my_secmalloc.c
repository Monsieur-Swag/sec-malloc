#define _GNU_SOURCE
#include "my_secmalloc.private.h"
#include <stdio.h>
#include <fcntl.h>
#include <alloca.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#include "test_utils.h"
#define _DEBUG 0
#if _DEBUG == 1
static size_t _DEBUG_VAUE = 0;
#define DEBUG() printf("DEBUG %zu\n", ++_DEBUG_VAUE);
#endif
#define PRINT(...) printf(__VA_ARGS__);putchar('\n');

// Reminder: NOTHING in this library is thread safe (i could just put a global mutex stored in the HEAP object)

struct heap HEAP = {
    .nbusyblocks = 0,
    .canary_thread = 0 // Is this usefull ?
};

#include <stdarg.h>

static void my_printf(const char* format,...) {
    va_list args;
    char buffer[1024]; // 1022 characters is enough for any logging message.
    buffer[1023] = '\0';
    buffer[1022] = '\0';

    va_start(args, format);
    int size = vsnprintf(buffer, 1022, format, args);
    va_end(args);
    buffer[size] = '\n';

    if (size >= 0)
        write(STDERR_FILENO,buffer,size+1);
}

static canary_t canary_generate() {
    // I have to execute 3 syscalls to generate a single canary :(.
    // I can either do batching or just ignore this performance optimization if the mmap syscall executes way slower than the the following consecutive syscalls. (i can always do some benchmark to check that)
    canary_t canary;
    fd_t fd = open(RANDOM_DEVICE,O_RDONLY | O_CLOEXEC);
    read(fd,&canary,sizeof(canary_t)); // The generate canary might fail here in some edge cases.
    // Should i implement errno return values instead of just returning -1 when something went wrong ?
    close(fd);
    return canary;
}



static void* heap_thread_function() {
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

// Using syscall(SYS_clone3,...); is better than using pthread because it removes the dependency to the libpthread.so.0 library.
static void heap_thread_init() {
    pthread_create(&HEAP.canary_thread,NULL,heap_thread_function,NULL); // Should i do error handling ?
}
static void heap_thread_stop() {
    pthread_cancel(HEAP.canary_thread); // Should i do error handling ?
    pthread_join(HEAP.canary_thread,NULL); // Should i do error handling ?
    HEAP.canary_thread = 0; // Is this usefull ?
}

static int32_t heap_init(size_t size) {
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

static struct block_entry* heap_get_free_block(size_t size) {
    // This function is not optimal.
    struct block_entry* block = HEAP.block_vector.start;
    for (size_t i=0;i<HEAP.block_vector.size;i++) {
        if (block->status == FREE && block->size >= size)
            return block;
        block++;
    }
    return NULL;
}

struct block_mprotect {
  void* address;
  size_t size;
};

static void block_mprotect(struct block_entry* block) {
    size_t protect_address = (size_t)(block->address) & ~(PAGE_SIZE-1);
    // size_t end_address = (size_t)(block->address + block->size) - ((size_t)(block->address + block->size) % PAGE_SIZE) + PAGE_SIZE;
    // size_t protect_size = end_address - protect_address;
    size_t protect_size = (size_t)(block->address - protect_address) + block->size;
    mprotect((void*)protect_address,protect_size,PROT_READ | PROT_WRITE);
}

static void block_set_canary(struct block_entry* block) {
    memcpy(block->address+block->size-sizeof(canary_t),&block->canary,sizeof(canary_t));
}

static void* heap_create_block_at_end(size_t size) {
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

static void* heap_create_block(size_t size) {
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

    // if (HEAP_FREE_SPACE < size)
        // heap_expand(size); // Should i do error handling there ? YES COMPLETLY

    return heap_create_block_at_end(size);
}

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

static int32_t heap_clear() {
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
static struct block_entry_indexed heap_get_block(void* ptr) {
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

    // int status = 
    mprotect(clean_addr,clean_size,PROT_NONE); // THIS MUST MEMORY ALIGNED !
    printf("[ALIGNEMENT] mprotect(%p, %lx)\n", clean_addr, clean_size);
    madvise(clean_addr,clean_size,MADV_DONTNEED);

    if (first_block->address+first_block->size == HEAP.end) {
        HEAP.end = first_block->address;
        vector_erase(&HEAP.block_vector,block_before != NULL ? block_ref->index : block_before_index);
    }

    HEAP.nbusyblocks--;
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
    if (addr != NULL && nmemb > 0 && size > 0)
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
    if (ptr == NULL)
        return my_malloc(size);

    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    /* struct block_entry_indexed block_entry = heap_get_block(ptr);
    if (ptr == NULL)
        return NULL;
    if (block_entry.block->size == size)
        return ptr
    
    if (block_entry.block->size > size) {

    } else {
        
    } */
    
    // If the size of the block if inferior do something and return ptr
    // If the size of the block is superior do something else and maybe return a new pointer

    return NULL;
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
