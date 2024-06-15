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

// I will have to check all of the MAP_* and MADV_* and MREMAP_ macros and what they do to make a better use of mmap in my allocation functions.

/* Test(my_alloc, simplest) {
    printf("-------------------------------------\n");
    void* a = my_malloc(4096);
    void* b = my_malloc(4096);
    void* c = my_malloc(4096);
    void* d = my_malloc(4096);
    void* e = my_malloc(4096);
    // printf("Pointers = A=%p B=%p C=%p D=%p E=%p\n", a, b, c, d, e);
    my_free(b);
    my_free(d);
    my_free(c);
    printf("-------------------------------------\n");
    printf("-------------------------------------\n");
    void* new2 = my_malloc(20000); // 4096*3
    // void* new = my_malloc(16384);
    show_maps();
    print_heap();
    // my_free(new);
    my_free(new2);
    my_free(a);my_free(e);
    printf("-------------------------------------\n");
} */

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
    printf("--- [my_realloc : lower_size] ---\n");
    void* a, *b, *c, *d, *e;
    a = my_malloc(10000);
    b = my_malloc(20000);
    c = my_malloc(30000);
    d = my_malloc(40000);
    e = my_malloc(50000);

    // print_heap();
    my_realloc(c,10000);
    // print_heap();

    my_free(a);
    my_free(c);
    my_free(b);
    my_free(d);
    my_free(e);
}

Test(my_realloc, same_size) {
    // printf("--- [my_realloc : same_size] ---\n");
    void* a, *b, *c, *d, *e;
    a = my_malloc(10000);
    b = my_malloc(20000);
    c = my_malloc(30000);
    d = my_malloc(40000);
    e = my_malloc(50000);

    // print_heap();
    my_realloc(c,30000);
    // print_heap();

    my_free(a);
    my_free(c);
    my_free(b);
    my_free(d);
    my_free(e);
}

Test(my_realloc, greater_size) {
    printf("--- [my_realloc : greater_size] ---\n");
    void* a, *b, *c, *d, *e;
    a = my_malloc(10000);
    b = my_malloc(20000);
    c = my_malloc(30000);
    d = my_malloc(40000);
    e = my_malloc(50000);

    // print_heap();
    my_realloc(c,60000);
    // print_heap();

    my_free(a);
    my_free(c);
    my_free(b);
    my_free(d);
    my_free(e);
}


