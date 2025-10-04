#include "../my_custom_mem.h"
#include "esp_heap_caps.h"



void *psram_malloc(size_t size) 
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void *psram_realloc(void *ptr, size_t size) 
{
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM |MALLOC_CAP_8BIT);
}


void psram_free(void *ptr) 
{
    heap_caps_free(ptr);
}


