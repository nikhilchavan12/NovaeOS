/* =============================================================================
 * NovaeOS - Heap Allocator (kernel/mm/heap.h)
 * Simple kernel-space malloc/free using a linked list of blocks
 * ============================================================================= */

#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

void  heap_init(void *start, size_t size);
void *kmalloc(size_t size);
void  kfree(void *ptr);
void *krealloc(void *ptr, size_t new_size);
void *kmemset(void *dst, int val, size_t n);
void *kmemcpy(void *dst, const void *src, size_t n);
int   kmemcmp(const void *a, const void *b, size_t n);

#endif /* HEAP_H */
