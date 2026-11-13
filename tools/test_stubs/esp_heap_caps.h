#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
size_t heap_caps_get_total_size(unsigned);
void *heap_caps_malloc(size_t,unsigned);
void *heap_caps_calloc(size_t,size_t,unsigned);
