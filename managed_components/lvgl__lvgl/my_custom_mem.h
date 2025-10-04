#pragma once
#include <string.h>


void *psram_malloc(size_t size);


void *psram_realloc(void *ptr, size_t size);


void psram_free(void *ptr);
