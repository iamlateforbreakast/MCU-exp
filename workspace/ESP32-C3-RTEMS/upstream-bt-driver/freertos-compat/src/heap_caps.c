#include "esp_heap_caps.h"
#include <stdlib.h>

void *heap_caps_malloc(size_t size, uint32_t caps)
{
    (void) caps;
    return malloc(size);
}

void *heap_caps_calloc(size_t n, size_t size, uint32_t caps)
{
    (void) caps;
    return calloc(n, size);
}

esp_err_t heap_caps_add_region(intptr_t start, intptr_t end)
{
    (void) start;
    (void) end;
    return ESP_OK;
}
