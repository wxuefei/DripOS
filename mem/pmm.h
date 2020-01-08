#ifndef PMM_H
#define PMM_H

#include <stdint.h>
extern uint64_t memory_remaining;
extern uint64_t used_mem;
extern uint64_t bitmap_size;
extern uint64_t free_mem_addr;
extern uint64_t MAX;
extern uint64_t MIN;
extern uint8_t *bitmap;

void set_addr(uint32_t addr, uint32_t mem_size);
uint64_t pmm_find_free(uint64_t size);
uint64_t pmm_allocate(uint64_t size);
void pmm_unallocate(void * address, uint64_t size);

#endif