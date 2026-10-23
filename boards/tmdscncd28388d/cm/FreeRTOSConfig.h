/*
 * FreeRTOS configuration for the F2838x Connectivity Manager on the
 * TMDSCNCD28388D controlCARD.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* Scheduler */
#define configUSE_PREEMPTION                        1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION     1
#define configUSE_TIME_SLICING                      1
#define configIDLE_SHOULD_YIELD                     1
#define configCPU_CLOCK_HZ                          125000000UL     /* CPU1 sets CMCLK = AUXPLL / 1 */
#define configTICK_RATE_HZ                          1000U
#define configTICK_TYPE_WIDTH_IN_BITS               TICK_TYPE_WIDTH_32_BITS
#define configMAX_PRIORITIES                        8U
#define configMINIMAL_STACK_SIZE                    128U            /* words */
#define configMAX_TASK_NAME_LEN                     12U
#define configSTACK_DEPTH_TYPE                      uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE            size_t
#define configENABLE_BACKWARD_COMPATIBILITY         0

/* Features */
#define configUSE_TASK_NOTIFICATIONS                1
#define configUSE_MUTEXES                           1
#define configUSE_RECURSIVE_MUTEXES                 0
#define configUSE_COUNTING_SEMAPHORES               1
#define configUSE_QUEUE_SETS                        0
#define configUSE_STREAM_BUFFERS                    1
#define configQUEUE_REGISTRY_SIZE                   0
#define configUSE_CO_ROUTINES                       0

/* Memory: heap_4, with the heap array provided by the board so that the
 * linker command file can place it in its own section (.freertos_heap). */
#define configSUPPORT_STATIC_ALLOCATION             1
#define configSUPPORT_DYNAMIC_ALLOCATION            1
#define configKERNEL_PROVIDED_STATIC_MEMORY         1
#define configAPPLICATION_ALLOCATED_HEAP            1
/* 01_publisher_udp needs 1.2 KB at most; an example's project can set its own. */
#ifndef configTOTAL_HEAP_SIZE
#define configTOTAL_HEAP_SIZE                       (4U * 1024U)
#endif

/* Hooks and checks */
#define configUSE_IDLE_HOOK                         0
#define configUSE_TICK_HOOK                         0
#define configUSE_MALLOC_FAILED_HOOK                1
#define configCHECK_FOR_STACK_OVERFLOW              2
#define configUSE_TRACE_FACILITY                    1
#define configGENERATE_RUN_TIME_STATS               0
#define configCHECK_HANDLER_INSTALLATION            1

/* Software timers */
#define configUSE_TIMERS                            1
#define configTIMER_TASK_PRIORITY                   (configMAX_PRIORITIES - 1U)
#define configTIMER_QUEUE_LENGTH                    8U
#define configTIMER_TASK_STACK_DEPTH                (configMINIMAL_STACK_SIZE * 2U)

/* Interrupt priorities: CM NVIC implements the top 3 priority bits.
 * ISRs that call FreeRTOS "FromISR" functions must have a numeric priority
 * >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5..7). */
#define configPRIO_BITS                             3U
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY     7U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U
#define configKERNEL_INTERRUPT_PRIORITY             (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY        (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

#define configASSERT(x)                                 \
    do {                                                \
        if ((x) == 0) {                                 \
            taskDISABLE_INTERRUPTS();                   \
            __asm volatile("bkpt #0");                  \
            for (;;) {                                  \
            }                                           \
        }                                               \
    } while (0)

/* Optional API */
#define INCLUDE_vTaskPrioritySet                    1
#define INCLUDE_uxTaskPriorityGet                   1
#define INCLUDE_vTaskDelete                         1
#define INCLUDE_vTaskSuspend                        1
#define INCLUDE_xTaskDelayUntil                     1
#define INCLUDE_vTaskDelay                          1
#define INCLUDE_xTaskGetSchedulerState              1
#define INCLUDE_xTaskGetCurrentTaskHandle           1
#define INCLUDE_uxTaskGetStackHighWaterMark         1
#define INCLUDE_xTaskGetIdleTaskHandle              0
#define INCLUDE_eTaskGetState                       0
#define INCLUDE_xTimerPendFunctionCall              0

#endif /* FREERTOS_CONFIG_H */
