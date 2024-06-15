#include <criterion/criterion.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>

#include "my_secmalloc.private.h"
#include "heap.h"
#include "test_utils.h"

Test(mmap, simple) {
    void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    cr_expect(ptr != NULL);
    int res = munmap(ptr, 4096);
    cr_expect(res == 0);
}

Test(vector, simple) {
    struct vector vector;

    cr_assert(vector_init(&vector,2,sizeof(uint32_t)) >= 0);

    uint32_t* values = vector.start;
    uint32_t data[5] = {10,20,30,40,50};

    cr_expect(vector.size == 0);
    cr_expect(vector.capacity == 2);
    cr_expect(vector.item_size == sizeof(uint32_t));

    vector_push(&vector,data);
    vector_push(&vector,data+1);
    vector_push(&vector,data+2);
    vector_push(&vector,data+3);
    vector_push(&vector,data+4);

    for (uint32_t i=0;i<5;i++)
        cr_expect(values[i] == data[i]);

    cr_expect(vector.size == 5);
    cr_expect(vector.capacity == 8);

    cr_expect(vector_erase(&vector,1) >= 0);
    cr_expect(vector_erase(&vector,4) >= 0);

    cr_expect(values[0] == data[0]);
    cr_expect(values[1] == data[4]);
    cr_expect(values[2] == data[2]);

    cr_expect(vector.size == 3);
    cr_expect(vector.capacity == 8);

    cr_expect(vector_clear(&vector) >= 0);
}

Test(vector, memory_size_flexibility) {
    struct vector vector;

    cr_assert(vector_init(&vector,16,sizeof(uint32_t)) >= 0);

    size_t buffer_size = PAGE_SIZE / 4;
    uint32_t buffer[buffer_size];
    for (uint32_t i=0;i<buffer_size;i++)
        buffer[i] = i;

    cr_expect(vector_make(&vector,buffer,buffer_size) >= 0);

    uint32_t value = 0xdeadbeef;
    vector_push(&vector, &value);
    cr_expect(vector.capacity == buffer_size*2);

    vector_erase(&vector,vector.size-1); // Either this line
    cr_expect(vector.capacity == buffer_size);

    cr_expect(vector_clear(&vector) >= 0);
}

// Make unit tests for double free : try with free(pointer) and free(pointer+1)

// Tests can have an output but they should be based on assert statements instead of letting a human judge is the test passed or not
Test(my_malloc, simple) {
    void* a, *b, *c, *d, *e;
    a = my_malloc(10000);
    b = my_malloc(20000);
    c = my_malloc(30000);
    d = my_malloc(40000);
    e = my_malloc(50000);

    /* sleep(2);
    void* pointer = a + 7;
    *(uint32_t*)(pointer) = 0x00000001;
    sleep(2); */

    cr_expect((HEAP.end - HEAP.start) == 150000 + 5*sizeof(canary_t)); // Verify that the HEAP memory space is contiguous

    my_free(a);
    my_free(c);

    struct block_entry* first_block = heap_get_free_block(5000);
    cr_expect(first_block->status == FREE && first_block->size == 10000 + sizeof(canary_t) && first_block->address == a); // Allocating (5000+8) bytes returns the (10000+8) block of pointed by a.
    struct block_entry* third_block = heap_get_free_block(15000);
    cr_expect(third_block->status == FREE && third_block->size == 30000 + sizeof(canary_t) && third_block->address == c); // Allocating (5000+8) bytes returns the (10000+8) block of pointed by c.

    my_free(b);

    struct block_entry* big_block = heap_get_free_block(5000);
    cr_expect(big_block->status == FREE && big_block->size == 60000 + 3*sizeof(canary_t) && big_block->address == a);

    my_free(d);
    my_free(e);

    big_block = heap_get_free_block(1000);
    cr_expect(big_block == NULL); // If all the heap memory is freed there is no free block left registered.
}

Test(my_realloc, lower_size) {
    void* a, *b, *c, *d, *e;
    a = my_malloc(10000);
    b = my_malloc(20000);
    c = my_malloc(30000);
    d = my_malloc(40000);
    e = my_malloc(50000);

    c = my_realloc(c,10000);

    struct block_entry_indexed c_block = heap_get_block(c);
    struct block_entry* free_block = heap_get_free_block(1000);
    cr_expect(free_block->address == c + c_block.block->size); // Check that the new free block if contiguous after the C block

    d = my_realloc(d,100000);

    // struct block_entry_indexed d_block = heap_get_block(c);
    free_block = heap_get_free_block(1000);
    cr_expect(free_block->address == c + c_block.block->size); // Check that the free block created by the C realloc and the free block created by the second D realloc are merged.

    free_block = heap_get_free_block(1000000000);
    cr_expect(free_block == NULL); // Check that heap_get_free_block returns NULL when no big enough free blocks is available for a specific size.

    my_free(a);
    my_free(c);
    my_free(b);
    my_free(d);
    my_free(e);

    free_block = heap_get_free_block(1);
    cr_expect(free_block == NULL); // If all the heap memory is freed there is no free block left registered.
}

Test(my_realloc, same_size) {
    void* a, *b, *c, *d, *e;
    a = my_malloc(10000);
    b = my_malloc(20000);
    c = my_malloc(30000);
    d = my_malloc(40000);
    e = my_malloc(50000);

    my_realloc(c,30000);

    my_free(a);
    my_free(c);
    my_free(b);
    my_free(d);
    my_free(e);
}

Test(my_calloc, greater_size) {
    void* a, *b, *c, *d, *e;
    a = my_calloc(1,10000);
    b = my_calloc(2,10000);
    c = my_calloc(3,10000);
    d = my_calloc(4,10000);
    e = my_calloc(5,10000);

    cr_expect((HEAP.end - HEAP.start) == 150000 + 5*sizeof(canary_t)); // Verify that the HEAP memory space is contiguous
    void* memory_allocs[5] = {a,b,c,d,e};
    char* memory_alloc;
    for (int i=0;i<5;i++) {
        memory_alloc = memory_allocs[i];
        for (int j=0;j<10000*(i+1);j++) {
            cr_assert(memory_alloc[j] == 0); // Check that all bytes except canaries are set to zero.
        }
    }

    my_free(a);
    my_free(c);
    my_free(b);
    my_free(d);
    my_free(e);
}


