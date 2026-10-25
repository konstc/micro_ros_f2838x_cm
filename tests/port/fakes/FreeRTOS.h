/*
 * Host-test stand-in for the FreeRTOS heap API used by the port's allocator.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef FAKE_FREERTOS_H
#define FAKE_FREERTOS_H

#include <stddef.h>

void* pvPortMalloc(size_t size);
void vPortFree(void* pointer);
size_t xPortGetMinimumEverFreeHeapSize(void);

#endif /* FAKE_FREERTOS_H */
