/*
 * FreeRTOS hooks shared by the examples.
 *
 * Both hooks stop the system in place so that the debugger shows the failing
 * task; the variables tell which task and why.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "FreeRTOS.h"
#include "task.h"

volatile const char* g_stackOverflowTask;

void vApplicationStackOverflowHook(TaskHandle_t task, char* taskName)
{
    (void)task;
    g_stackOverflowTask = taskName;
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
