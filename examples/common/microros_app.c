/*
 * Shared main() and micro-ROS task for the examples (see microros_app.h).
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "microros_app.h"

#include "FreeRTOS.h"
#include "task.h"

#include <rmw_microros/rmw_microros.h>

#include "board.h"

#ifndef APP_UROS_STACK_WORDS
#define APP_UROS_STACK_WORDS                                            (1536U)
#endif
#define UROS_TASK_STACK_WORDS                            (APP_UROS_STACK_WORDS)
#define UROS_TASK_PRIORITY                              (tskIDLE_PRIORITY + 2U)

/* while connected */
#define AGENT_PING_PERIOD_MS                                            (1000U)
#define AGENT_PING_TIMEOUT_MS                                             (100)
#define AGENT_PING_ATTEMPTS                                                 (3)
/* LED toggle and agent ping while waiting */
#define WAITING_PERIOD_MS                                                (500U)
#define SPIN_TIMEOUT_MS                                                   (10U)

static StaticTask_t s_urosTask;
static StackType_t s_urosStack[UROS_TASK_STACK_WORDS] BOARD_TASK_STACK;

/* rclc_support_init() and rclc_executor_init() keep a pointer to the
 * allocator, so it must outlive them. */
static rcl_allocator_t s_allocator;
static rclc_support_t s_support;
static rcl_node_t s_node;
static rclc_executor_t s_executor;

static void set_state(microros_f2838x_state_t state)
{
    microros_f2838x_stats.state = state;
}

/* Support, node, executor and the example's entities. On failure, whatever
 * was created is destroyed again, in reverse order. */
static bool create_entities(void)
{
    bool ok = false;

    s_allocator = rcl_get_default_allocator();

    if (rclc_support_init(&s_support, 0, NULL, &s_allocator) == RCL_RET_OK)
    {
        if (rclc_node_init_default(&s_node, app_node_name, "", &s_support) == RCL_RET_OK)
        {
            s_executor = rclc_executor_get_zero_initialized_executor();
            if (rclc_executor_init(&s_executor, &s_support.context, app_executor_handles,
                                   &s_allocator) == RCL_RET_OK)
            {
                /* rclc_executor_prepare() allocates the wait set now rather than in
                 * the first spin, so nothing is allocated once the allocations are
                 * locked. */
                ok = app_create(&s_node, &s_support, &s_executor) &&
                     (rclc_executor_prepare(&s_executor) == RCL_RET_OK);
                if (!ok)
                {
                    app_destroy(&s_node);
                    (void)rclc_executor_fini(&s_executor);
                }
            }
            if (!ok)
            {
                (void)rcl_node_fini(&s_node);
            }
        }
        if (!ok)
        {
            (void)rclc_support_fini(&s_support);
        }
    }

    return ok;
}

static void destroy_entities(void)
{
    /* The agent may be gone: do not wait for it to confirm the deletions. */
    rmw_context_t* rmwContext = rcl_context_get_rmw_context(&s_support.context);
    (void)rmw_uros_set_context_entity_destroy_session_timeout(rmwContext, 0);

    app_destroy(&s_node);
    (void)rclc_executor_fini(&s_executor);
    (void)rcl_node_fini(&s_node);
    (void)rclc_support_fini(&s_support);
}

static void fail(void)
{
    set_state(MICROROS_F2838X_STATE_ERROR);
    for (;;)
    {
        board_led_toggle(BOARD_LED_STATUS);
        vTaskDelay(pdMS_TO_TICKS(100U));
    }
}

static void uros_task(void* arg)
{
    (void)arg;

    if (!microros_f2838x_init(&app_config))
    {
        fail();
    }

    microros_f2838x_state_t state = MICROROS_F2838X_STATE_WAITING_AGENT;
    TickType_t lastPing = xTaskGetTickCount();

    for (;;)
    {
        set_state(state);
        switch (state)
        {
            case MICROROS_F2838X_STATE_WAITING_AGENT:
                board_led_toggle(BOARD_LED_STATUS);
                if (rmw_uros_ping_agent(AGENT_PING_TIMEOUT_MS, 1) == RMW_RET_OK)
                {
                    state = MICROROS_F2838X_STATE_AGENT_AVAILABLE;
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(WAITING_PERIOD_MS));
                }
                break;

            case MICROROS_F2838X_STATE_AGENT_AVAILABLE:
                if (create_entities())
                {
                    board_led_set(BOARD_LED_STATUS, true);
                    microros_f2838x_lock_allocations(true);
                    lastPing = xTaskGetTickCount();
                    state = MICROROS_F2838X_STATE_CONNECTED;
                }
                else
                {
                    state = MICROROS_F2838X_STATE_WAITING_AGENT;
                }
                break;

            case MICROROS_F2838X_STATE_CONNECTED:
            {
                bool agentLost = false;

                if ((xTaskGetTickCount() - lastPing) >= pdMS_TO_TICKS(AGENT_PING_PERIOD_MS))
                {
                    lastPing = xTaskGetTickCount();
                    agentLost = (rmw_uros_ping_agent(AGENT_PING_TIMEOUT_MS, AGENT_PING_ATTEMPTS)
                                 != RMW_RET_OK);
                }

                if (agentLost)
                {
                    state = MICROROS_F2838X_STATE_DISCONNECTED;
                }
                else
                {
                    (void)rclc_executor_spin_some(&s_executor, RCL_MS_TO_NS(SPIN_TIMEOUT_MS));
                    microros_f2838x_stats.uros_stack_free_min_words =
                        (uint32_t)uxTaskGetStackHighWaterMark(NULL);
                }
                break;
            }

            case MICROROS_F2838X_STATE_DISCONNECTED:
            default:
                microros_f2838x_lock_allocations(false);
                destroy_entities();
                board_led_set(BOARD_LED_STATUS, false);
                microros_f2838x_stats.reconnects++;
                state = MICROROS_F2838X_STATE_WAITING_AGENT;
                break;
        }
    }
}

int main(void)
{
    board_init();

    (void)xTaskCreateStatic(uros_task, "uros", UROS_TASK_STACK_WORDS, NULL, UROS_TASK_PRIORITY,
                            s_urosStack, &s_urosTask);
    vTaskStartScheduler();

    /* Only reached if the scheduler could not start. */
    for (;;)
    {
    }
}
