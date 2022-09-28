#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

#define CHUNK_SIZE (1<<12)

extern void *bulk_alloc(size_t size);

extern void bulk_free(void *ptr, size_t size);

static inline __attribute__((unused)) int block_index(size_t x) {
    if (x <= 8) {
        return 5;
    } else {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
} 

/*
 *  block struct
 */

typedef struct block {
    struct block *next;
} Block;

static Block *table[13] = {NULL};

void *malloc(size_t size) {

    // check if size is 0
    if (size <= 0) {
        return NULL;
    }

    // check if size greater than (CHUNK_SIZE - 8)
    if (size <= (CHUNK_SIZE - 8)) {

        int index = block_index(size);

        // check if this power has free blocks
        if (table[index] == NULL) {

            // ptr points to the beginning of 4096 chunk
            void *ptr = sbrk(CHUNK_SIZE);

            // // connect first block with table
            // table[index] = (Block *)ptr;
            // // table[index] -> header = (1<<index);
            // table[index] -> next = NULL;

            // the number of blocks
            int num = CHUNK_SIZE/(1<<index);

            // insert all the blocks with table
            for (int i = 0; i < num; i++) {
                Block *prev = table[index];
                *(size_t *)ptr = (1<<index); // header
                ptr += 8; // skip header
                table[index] = (Block *)ptr;
                table[index] -> next = prev;
                ptr += ((1<<index) - 8); // skip the real available size
            }

        }

        // take out the first block of the linked list
        Block *target = table[index];
        // *(size_t *)(target - 8) = (1<<index) | 0x1; // flag
        table[index] = target -> next;

        return target; // in here, the pointer is already points to after the header

    } else {
        void *bulk_ptr = bulk_alloc(size + 8);
        *(size_t *)bulk_ptr = size + 8; // plus the header
        return bulk_ptr + 8; // skip the header
    }
}

void *calloc(size_t nmemb, size_t size) {

    // check if they are 0, return NULL
    if(nmemb == 0 || size == 0){
        return NULL;
    }

    void *ptr = malloc(nmemb * size);
    memset(ptr, 0, nmemb * size);
    return ptr;
}

void *realloc(void *ptr, size_t size) {

    // check if ptr is NULL, treat it as malloc()
    if (ptr == NULL) {
        return malloc(size);
    }

    // check if size is 0, return NULL
    if (size == 0) {
        return NULL;
    }

    // check if the size requesting is less/equal/greater than the size of old object
    size_t old_size = *(size_t *)(ptr - 8);

        // old_size contains the header, the actual available size is old_size - 8
    if (old_size - 8 >= size) {
        return ptr;
    } else {
        void *dest = malloc(size);
        memcpy(dest, ptr, old_size - 8);
        free(ptr);
        return dest; // exclude the header
    }
}

void free(void *ptr) {

    // check if ptr is a NULL pointer, return nothing
    if (ptr == NULL) {
        return;
    }

    // size_t flagSize = *(size_t *)(ptr - 8);
    // *(size_t *)(ptr - 8) = flagSize & ~0x1; // un-flag

    // check the size to determine the free function
    size_t size = *(size_t *)(ptr - 8);

    if (size > CHUNK_SIZE) {
        bulk_free(ptr - 8, size);
        // yong bu yong free header?
    } else {
        int index = block_index(size - 8);
        Block *prev = table[index];
        table[index] = (Block *)(ptr - 8);
        table[index] -> next = prev;
    }
}
