/* =============================================================================
 * NovaeOS - Physical Memory Manager (kernel/mm/pmm.c)
 * Bitmap-based 4KB page allocator
 * ============================================================================= */

#include "pmm.h"
#include <stdint.h>
#include <stddef.h>

static uint32_t *pmm_bitmap    = 0;
static size_t    total_blocks  = 0;
static size_t    free_blocks   = 0;

/* ---- Bitmap helpers ---- */
static inline void bitmap_set(size_t bit) {
    pmm_bitmap[bit / 32] |= (1U << (bit % 32));
}
static inline void bitmap_clear(size_t bit) {
    pmm_bitmap[bit / 32] &= ~(1U << (bit % 32));
}
static inline int bitmap_test(size_t bit) {
    return (pmm_bitmap[bit / 32] >> (bit % 32)) & 1;
}

/* Find first free block using bitmap scan */
static int bitmap_first_free(void) {
    for (size_t i = 0; i < total_blocks / 32; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (int j = 0; j < 32; j++) {
                if (!((pmm_bitmap[i] >> j) & 1))
                    return (int)(i * 32 + j);
            }
        }
    }
    return -1;
}

void pmm_init(uint32_t mem_size_kb, uint32_t *bitmap_addr) {
    pmm_bitmap   = bitmap_addr;
    total_blocks = (mem_size_kb * 1024) / PMM_BLOCK_SIZE;
    free_blocks  = total_blocks;

    /* Mark all memory as used initially */
    for (size_t i = 0; i < total_blocks / 32 + 1; i++)
        pmm_bitmap[i] = 0xFFFFFFFF;
}

/* Mark a 4KB block at physical address 'base' as free */
static void pmm_mark_region_free(uint32_t base, uint32_t size) {
    size_t start_block = base / PMM_BLOCK_SIZE;
    size_t block_count = size / PMM_BLOCK_SIZE;
    for (size_t i = 0; i < block_count; i++) {
        bitmap_clear(start_block + i);
        free_blocks++;
    }
}

/* Mark region as used (e.g. kernel, reserved memory) */
static void pmm_mark_region_used(uint32_t base, uint32_t size) {
    size_t start_block = base / PMM_BLOCK_SIZE;
    size_t block_count = (size + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;
    for (size_t i = 0; i < block_count; i++) {
        if (!bitmap_test(start_block + i)) {
            bitmap_set(start_block + i);
            if (free_blocks > 0) free_blocks--;
        }
    }
}

void *pmm_alloc_block(void) {
    if (free_blocks == 0) return 0;
    int block = bitmap_first_free();
    if (block == -1) return 0;
    bitmap_set((size_t)block);
    free_blocks--;
    return (void *)((size_t)block * PMM_BLOCK_SIZE);
}

void pmm_free_block(void *p) {
    size_t block = (size_t)p / PMM_BLOCK_SIZE;
    bitmap_clear(block);
    free_blocks++;
}

size_t pmm_get_free_blocks(void)  { return free_blocks; }
size_t pmm_get_total_blocks(void) { return total_blocks; }
