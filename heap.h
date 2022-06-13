#ifndef ZAD1_8_HEAP_H
#define ZAD1_8_HEAP_H

#include <stdlib.h>
#define HEAP_SIZE 4096
#define FENCE_SIZE sizeof(void *)
#define SIZEOF_HEAP sizeof(struct heap_t)
#define SIZEOF_BLOCK sizeof(struct block_t)
#define PTR_SIZE sizeof(void *)
#define PAGE_SIZE 4096

#define ALIGMENT 8
#define ALIGN(x) (((x) + (ALIGMENT - 1)) & ~(ALIGMENT - 1))
#define ALIGN_TO_PAGE(x) ((void *)((((uint64_t)(x)) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1)))

#define COUNT_CHECKSUM(x) ((x)->block_size + (int)((((uint64_t *)(x)->next - (uint64_t *)(x)->prev) >> 32) + ((uint32_t *)(x)->next - (uint32_t *)(x)->prev)) + (x)->block_size + (x)->magic1)

#define MAGIC1 (int)0x123456
#define MAGIC2 (int)0x654321

struct heap_t{
    struct block_t *head;
    struct block_t *tail;
    size_t heap_size;
};

struct __attribute__((packed)) block_t{
    int magic1;
    struct block_t* next;
    struct block_t* prev;
    int block_size;         //block_size < 0 ->wolne,  block_size > 0 ->zajete
    int check_sum;    //suma bitowa ze zmiennych powyzej
    int magic2;
};

struct heap_t *heap;

enum pointer_type_t
{
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

int heap_show(int flag);

int heap_setup(void);
void heap_clean(void);

int heap_increase(size_t size);

void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t count);
void  heap_free(void* memblock);
int heap_validate(void);
int good_address(void *address);

size_t heap_get_largest_used_block_size(void);
enum pointer_type_t get_pointer_type(const void* const pointer);

void* heap_malloc_aligned(size_t count);
void* heap_calloc_aligned(size_t number, size_t size);
void* heap_realloc_aligned(void* memblock, size_t size);


void* heap_malloc_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename);

void* heap_malloc_aligned_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline, const char* filename);

#endif //ZAD1_8_HEAP_H
