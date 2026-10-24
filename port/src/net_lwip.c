/*
 * lwIP network task (NO_SYS=1, see net_lwip.h).
 *
 * The board's Ethernet interrupts only notify this task; the task drains the
 * receive and transmit queues (board_eth_service), runs the lwIP timers and
 * sleeps until the next event or timer. This file also provides lwIP's
 * system hooks for NO_SYS: sys_now() and the interrupt protection used by
 * SYS_LIGHTWEIGHT_PROT.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "net_lwip.h"

#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "lwip/init.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"

#include "microros_f2838x/board_api.h"
#include "microros_f2838x/microros_f2838x.h"
#include "microros_f2838x/microros_f2838x_config.h"

#define NET_START_TIMEOUT_MS                                            (5000U)

static StaticTask_t s_taskBuffer;
static StackType_t s_taskStack[MICROROS_F2838X_NET_TASK_STACK_WORDS]
    __attribute__((section(".task_stacks"), aligned(8)));

static TaskHandle_t s_task;
static TaskHandle_t s_starter;

static StaticSemaphore_t s_lockBuffer;
static SemaphoreHandle_t s_lock;

static struct netif s_netif;
static ip4_addr_t s_ip;
static ip4_addr_t s_netmask;
static ip4_addr_t s_gateway;
static uint8_t s_mac[6];

void net_lock(void)
{
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
}

void net_unlock(void)
{
    (void)xSemaphoreGive(s_lock);
}

/* Called by the board from interrupt context. */
static void eth_event_from_isr(void)
{
    BaseType_t woken = pdFALSE;

    if (s_task != NULL)
    {
        vTaskNotifyGiveFromISR(s_task, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

static void net_task(void* arg)
{
    (void)arg;

    /* This task has the higher priority and may run before xTaskCreateStatic()
     * has returned the handle to net_start(). */
    s_task = xTaskGetCurrentTaskHandle();

    net_lock();
    lwip_init();
    board_eth_configure(s_mac, eth_event_from_isr);
    bool up = (netif_add(&s_netif, &s_ip, &s_netmask, &s_gateway, NULL,
                         board_eth_netif_init, ethernet_input) != NULL);
    if (up)
    {
        netif_set_default(&s_netif);
        netif_set_up(&s_netif);
    }
    net_unlock();

    xTaskNotify(s_starter, up ? 1U : 2U, eSetValueWithOverwrite);
    if (!up)
    {
        vTaskSuspend(NULL);
    }

    uint32_t sleepMs = 0U;
    for(;;)
    {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(sleepMs));

        net_lock();
        board_eth_service(&s_netif);
        sys_check_timeouts();
        sleepMs = sys_timeouts_sleeptime();
        net_unlock();

        if (sleepMs > MICROROS_F2838X_NET_MAX_SLEEP_MS)
        {
            sleepMs = MICROROS_F2838X_NET_MAX_SLEEP_MS;
        }
        microros_f2838x_stats.net_stack_free_min_words = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
    }
}

bool net_start(const char* ip, const char* netmask, const char* gateway, const uint8_t mac[6])
{
    bool ret = false;

    if ((ip != NULL) && (netmask != NULL) && (gateway != NULL) &&
        ip4addr_aton(ip, &s_ip) && ip4addr_aton(netmask, &s_netmask) &&
        ip4addr_aton(gateway, &s_gateway))
    {
        memcpy(s_mac, mac, sizeof(s_mac));
        s_lock = xSemaphoreCreateMutexStatic(&s_lockBuffer);
        s_starter = xTaskGetCurrentTaskHandle();
        (void)xTaskNotifyStateClear(NULL);
        (void)xTaskCreateStatic(net_task, "net", MICROROS_F2838X_NET_TASK_STACK_WORDS, NULL,
                                MICROROS_F2838X_NET_TASK_PRIORITY, s_taskStack, &s_taskBuffer);

        uint32_t result = 0U;
        (void)xTaskNotifyWait(0U, UINT32_MAX, &result, pdMS_TO_TICKS(NET_START_TIMEOUT_MS));
        ret = result == 1U;
    }
    
    return ret;
}

/* ------------------------------ lwIP hooks ------------------------------- */

u32_t sys_now(void)
{
    return (u32_t)(board_time_us() / 1000U);
}

/* SYS_LIGHTWEIGHT_PROT: short critical sections inside lwIP (pools, pbufs). */
sys_prot_t sys_arch_protect(void)
{
    uint32_t primask;

    __asm volatile("mrs %0, primask\n"
                   "cpsid i" : "=r"(primask) : : "memory");
    return (sys_prot_t)primask;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    if ((pval & 1U) == 0U)
    {
        __asm volatile("cpsie i" : : : "memory");
    }
}
