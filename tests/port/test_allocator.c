/*
 * Host tests for port/src/allocators_freertos.c.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "allocators.h"
#include "microros_f2838x/microros_f2838x.h"

static int s_failures;
static int s_mallocFailAfter = -1;      /* fail the n-th next pvPortMalloc; -1 = never */
static int s_liveBlocks;

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);     \
            s_failures++;                                                       \
        }                                                                       \
    } while (0)

void* pvPortMalloc(size_t size)
{
    void* p = NULL;

    if (s_mallocFailAfter == 0)
    {
        s_mallocFailAfter = -1;
    }
    else
    {
        if (s_mallocFailAfter > 0)
        {
            s_mallocFailAfter--;
        }
        p = malloc(size);
        s_liveBlocks += (p != NULL);
    }

    return p;
}

void vPortFree(void* pointer)
{
    s_liveBlocks -= (pointer != NULL);
    free(pointer);
}

size_t xPortGetMinimumEverFreeHeapSize(void)
{
    return 1234U;
}

static void reset_stats(void)
{
    memset((void*)&microros_f2838x_stats, 0, sizeof(microros_f2838x_stats));
}

static void test_alignment_and_accounting(void)
{
    reset_stats();
    void* a = microros_f2838x_allocate(3U, NULL);
    void* b = microros_f2838x_allocate(100U, NULL);
    CHECK(a != NULL && b != NULL);
    CHECK(((uintptr_t)a % 8U) == 0U);
    CHECK(((uintptr_t)b % 8U) == 0U);
    CHECK(microros_f2838x_stats.heap_in_use == 103U);
    CHECK(microros_f2838x_stats.heap_free_min == 1234U);

    microros_f2838x_deallocate(a, NULL);
    CHECK(microros_f2838x_stats.heap_in_use == 100U);
    CHECK(microros_f2838x_stats.heap_in_use_peak == 103U);
    microros_f2838x_deallocate(b, NULL);
    microros_f2838x_deallocate(NULL, NULL);
    CHECK(microros_f2838x_stats.heap_in_use == 0U);
    CHECK(s_liveBlocks == 0);
}

static void test_reallocate(void)
{
    reset_stats();
    uint8_t* p = microros_f2838x_reallocate(NULL, 4U, NULL);    /* acts as allocate */
    CHECK(p != NULL);
    memcpy(p, "abcd", 4U);

    uint8_t* q = microros_f2838x_reallocate(p, 64U, NULL);      /* grow: content kept */
    CHECK(q != NULL && memcmp(q, "abcd", 4U) == 0);
    CHECK(microros_f2838x_stats.heap_in_use == 64U);

    uint8_t* r = microros_f2838x_reallocate(q, 2U, NULL);       /* shrink */
    CHECK(r != NULL && memcmp(r, "ab", 2U) == 0);
    CHECK(microros_f2838x_stats.heap_in_use == 2U);

    s_mallocFailAfter = 0;                                      /* failure: old block stays */
    CHECK(microros_f2838x_reallocate(r, 128U, NULL) == NULL);
    CHECK(memcmp(r, "ab", 2U) == 0);
    CHECK(microros_f2838x_stats.heap_in_use == 2U);

    CHECK(microros_f2838x_reallocate(r, 0U, NULL) == NULL);     /* size 0 frees */
    CHECK(microros_f2838x_stats.heap_in_use == 0U);
    CHECK(s_liveBlocks == 0);
}

static void test_zero_allocate(void)
{
    reset_stats();
    uint8_t* p = microros_f2838x_zero_allocate(10U, 7U, NULL);
    CHECK(p != NULL);
    int allZero = 1;
    for (int i = 0; i < 70; i++)
    {
        allZero &= (p[i] == 0U);
    }
    CHECK(allZero);
    microros_f2838x_deallocate(p, NULL);

    CHECK(microros_f2838x_zero_allocate(SIZE_MAX / 2U, 3U, NULL) == NULL);   /* overflow */
    CHECK(microros_f2838x_allocate(SIZE_MAX, NULL) == NULL);
    CHECK(s_liveBlocks == 0);
}

static void test_lock_counting(void)
{
    reset_stats();
    void* a = microros_f2838x_allocate(8U, NULL);
    CHECK(microros_f2838x_stats.allocs_while_locked == 0U);

    microros_f2838x_allocator_lock(true);
    void* b = microros_f2838x_allocate(8U, NULL);
    void* c = microros_f2838x_reallocate(b, 16U, NULL);
    CHECK(microros_f2838x_stats.allocs_while_locked == 2U);
    microros_f2838x_allocator_lock(false);

    void* d = microros_f2838x_allocate(8U, NULL);
    CHECK(microros_f2838x_stats.allocs_while_locked == 2U);

    microros_f2838x_deallocate(a, NULL);
    microros_f2838x_deallocate(c, NULL);
    microros_f2838x_deallocate(d, NULL);
    CHECK(s_liveBlocks == 0);
}

int main(void)
{
    test_alignment_and_accounting();
    test_reallocate();
    test_zero_allocate();
    test_lock_counting();

    printf("test_allocator: %s\n", s_failures ? "FAILED" : "passed");
    return s_failures ? 1 : 0;
}
