/* =============================================================================
 * NovaeOS - Heap Allocator (kernel/mm/heap.c)
 * Simple first-fit linked-list heap allocator + memory utilities
 * ============================================================================= */

#include "heap.h"
#include <stdint.h>
#include <stddef.h>

/* Heap block header - stored immediately before each allocation */
typedef struct heap_block {
    size_t             size;   /* Size of usable data area (not including header) */
    int                free;   /* 1 if this block is free */
    struct heap_block *next;   /* Next block in list */
} heap_block_t;

#define HEADER_SIZE sizeof(heap_block_t)
#define MIN_SPLIT   (HEADER_SIZE + 16)  /* Min useful block to split off */

static heap_block_t *heap_start = NULL;

/* ============================================================================
 * Memory utility functions (no libc)
 * ============================================================================ */

void *kmemset(void *dst, int val, size_t n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--) *p++ = (uint8_t)val;
    return dst;
}

void *kmemcpy(void *dst, const void *src, size_t n) {
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

int kmemcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++; pb++;
    }
    return 0;
}

/* ============================================================================
 * Heap initialization
 * ============================================================================ */

void heap_init(void *start, size_t size) {
    heap_start       = (heap_block_t *)start;
    heap_start->size = size - HEADER_SIZE;
    heap_start->free = 1;
    heap_start->next = NULL;
}

/* ============================================================================
 * Merge adjacent free blocks (defragmentation)
 * ============================================================================ */

static void heap_coalesce(void) {
    heap_block_t *cur = heap_start;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

/* ============================================================================
 * kmalloc - First-fit allocator
 * ============================================================================ */

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    /* Align size to 8 bytes */
    size = (size + 7) & ~7U;

    heap_block_t *cur = heap_start;
    while (cur) {
        if (cur->free && cur->size >= size) {
            /* Split block if there's enough room */
            if (cur->size >= size + MIN_SPLIT) {
                heap_block_t *new_block = (heap_block_t *)((uint8_t *)cur + HEADER_SIZE + size);
                new_block->size = cur->size - size - HEADER_SIZE;
                new_block->free = 1;
                new_block->next = cur->next;
                cur->next = new_block;
                cur->size = size;
            }
            cur->free = 0;
            return (void *)((uint8_t *)cur + HEADER_SIZE);
        }
        cur = cur->next;
    }
    return NULL;  /* Out of memory */
}

/* ============================================================================
 * kfree
 * ============================================================================ */

void kfree(void *ptr) {
    if (!ptr) return;
    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - HEADER_SIZE);
    block->free = 1;
    heap_coalesce();
}

/* ============================================================================
 * krealloc
 * ============================================================================ */

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr)      return kmalloc(new_size);
    if (!new_size) { kfree(ptr); return NULL; }

    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - HEADER_SIZE);
    if (block->size >= new_size) return ptr;

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;
    kmemcpy(new_ptr, ptr, block->size);
    kfree(ptr);
    return new_ptr;
}
