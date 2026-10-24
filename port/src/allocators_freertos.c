/*
 * rcl allocator on the FreeRTOS heap (heap_4).
 *
 * heap_4 has no realloc and does not report block sizes, so every block
 * carries an 8-byte header with the requested size; 8 bytes keep the
 * payload at heap_4's 8-byte alignment. The sizes feed the statistics:
 * bytes in use, peak, and allocations made while allocations are "locked"
 * (the executor loop is expected not to allocate).
 *
 * rcl calls the allocator from the micro-ROS task only.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "allocators.h"

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"

#include "microros_f2838x/microros_f2838x.h"

typedef union
{
    size_t size;
    uint64_t align;
} block_header_t;

static bool s_locked;

static void account(size_t added, size_t removed)
{
    uint32_t inUse = microros_f2838x_stats.heap_in_use + (uint32_t)added - (uint32_t)removed;

    microros_f2838x_stats.heap_in_use = inUse;

    if (inUse > microros_f2838x_stats.heap_in_use_peak)
    {
        microros_f2838x_stats.heap_in_use_peak = inUse;
    }

    microros_f2838x_stats.heap_free_min = (uint32_t)xPortGetMinimumEverFreeHeapSize();
}

void* microros_f2838x_allocate(size_t size, void* state)
{
    (void)state;
    void* ret = NULL;

    if (size <= (SIZE_MAX - sizeof(block_header_t)))
    {
        block_header_t* header = pvPortMalloc(sizeof(block_header_t) + size);
        if (header != NULL)
        {
            header->size = size;
            if (s_locked)
            {
                microros_f2838x_stats.allocs_while_locked++;
            }
            account(size, 0U);
            ret = header + 1;
        }
    }

    return ret;
}

void microros_f2838x_deallocate(void* pointer, void* state)
{
    (void)state;

    if (pointer != NULL)
    {
        block_header_t *header = (block_header_t *)pointer - 1;
        account(0U, header->size);
        vPortFree(header);
    }
}

void* microros_f2838x_reallocate(void* pointer, size_t size, void* state)
{
    void* ret = NULL;

    if (pointer == NULL)
    {
        ret = microros_f2838x_allocate(size, state);
    }
    else if (size == 0U)
    {
        microros_f2838x_deallocate(pointer, state);
    }
    else
    {
        size_t oldSize = ((block_header_t *)pointer - 1)->size;
        void *newPointer = microros_f2838x_allocate(size, state);

        /* like realloc: if the allocation fails, the old block stays valid */
        if (newPointer != NULL)
        {
            memcpy(newPointer, pointer, (oldSize < size) ? oldSize : size);
            microros_f2838x_deallocate(pointer, state);
            ret = newPointer;
        }
    }

    return ret;
}

void* microros_f2838x_zero_allocate(size_t count, size_t size, void* state)
{
    void* ret = NULL;

    if ((size == 0U) || (count <= (SIZE_MAX / size)))
    {
        void *pointer = microros_f2838x_allocate(count * size, state);
        if (pointer != NULL)
        {
            memset(pointer, 0, count * size);
        }
        ret = pointer;
    }

    return ret;
}

void microros_f2838x_allocator_lock(bool locked)
{
    s_locked = locked;
}
