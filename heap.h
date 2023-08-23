//
// Created by root on 12/10/22.
//
#ifndef PROJECT1_HEAP_H
#define PROJECT1_HEAP_H
#include <stdlib.h>
#include "custom_unistd.h"
#include <stdio.h>
#include <string.h>
#define FENCE_SIZE 8
#define PAGE_SIZE 4096

union pointer
{
    int8_t bytes[8];
    int64_t size ;
};


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
struct heap_mem
{
    struct heap_mem *prev;
    struct heap_mem *next;
    int64_t size;
    int64_t checksum;

};

struct heap_mem *heapMem;

int heap_setup(void);


void heap_clean(void);
void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t count);
void  heap_free(void* memblock);
size_t   heap_get_largest_used_block_size(void);
enum pointer_type_t get_pointer_type(const void* const pointer);
int heap_validate(void);
#endif //PROJECT1_HEAP_H
