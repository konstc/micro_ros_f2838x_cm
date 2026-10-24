/*
 * rcl allocator on the FreeRTOS heap (internal to the port).
 *
 * The functions have the signatures of rcutils_allocator_t.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MICROROS_F2838X_ALLOCATORS_H
#define MICROROS_F2838X_ALLOCATORS_H

#include <stdbool.h>
#include <stddef.h>

void* microros_f2838x_allocate(size_t size, void* state);
void microros_f2838x_deallocate(void* pointer, void* state);
void* microros_f2838x_reallocate(void* pointer, size_t size, void* state);
void* microros_f2838x_zero_allocate(size_t count, size_t size, void* state);

void microros_f2838x_allocator_lock(bool locked);

#endif /* MICROROS_F2838X_ALLOCATORS_H */
