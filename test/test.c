#include <criterion/criterion.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>

#include "my_secmalloc.private.h"
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

void print_heap() {
    printf("----- [HEAP STATE] -----\n");
    printf("- start=%p end=%p size=%lx\n", HEAP.start, HEAP.end, (HEAP.end - HEAP.start));
    struct block_entry* block = HEAP.block_vector.start;
    for (size_t i=0;i<HEAP.block_vector.size;i++) {
        printf("[BLOCK %zu] addr=%p size=%zu, status=%d\n", i, block->address, block->size, block->status);
        block++;
    }
    printf("----- [HEAP STATE] -----\n");
}

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

    my_free(a);
    my_free(c);
    my_free(b);
    my_free(d);
    my_free(e);
}


