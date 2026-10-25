/* =============================================================================
 * NovaeOS - Memory Manager (kernel/mm/pmm.h)
 * Physical Memory Manager - bitmap allocator
 * ============================================================================= */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PMM_BLOCK_SIZE  4096   /* 4 KB pages */

void pmm_init(uint32_t mem_size_kb, uint32_t *bitmap_addr);
void *pmm_alloc_block(void);
void  pmm_free_block(void *p);
size_t pmm_get_free_blocks(void);
size_t pmm_get_total_blocks(void);
void   pmm_mark_region_used(uint32_t base, uint32_t size);

#endif /* PMM_H */
