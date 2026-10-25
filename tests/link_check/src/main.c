/*
 * link_check: links libmicroros.a into a CM image together with FreeRTOS,
 * driverlib_cm and the port's libc shims, using the API surface the examples
 * need.
 *
 * Its purpose is the build: no undefined symbols, and a realistic flash/RAM
 * footprint in the map file. On the board it runs with a stub transport that
 * never reaches an agent.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rclc_parameter/rclc_parameter.h>
#include <rmw_microros/rmw_microros.h>
#include <std_msgs/msg/int32.h>
#include <std_srvs/srv/trigger.h>

#include "board.h"
#include "microros_f2838x_build_info.h"

#define MICROROS_TASK_STACK_WORDS   2048U   /* 8 KB */

static StaticTask_t s_microrosTask;
static StackType_t s_microrosStack[MICROROS_TASK_STACK_WORDS] BOARD_TASK_STACK;

/* ---------------------------- stub transport ----------------------------- */

static bool stub_open(struct uxrCustomTransport* transport)
{
    (void)transport;
    return true;
}

static bool stub_close(struct uxrCustomTransport* transport)
{
    (void)transport;
    return true;
}

static size_t stub_write(struct uxrCustomTransport* transport, const uint8_t* buf, size_t len, uint8_t* err)
{
    (void)transport;
    (void)buf;
    (void)err;
    return len;
}

static size_t stub_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err)
{
    (void)transport;
    (void)buf;
    (void)len;
    (void)err;
    vTaskDelay(pdMS_TO_TICKS(timeout > 0 ? timeout : 1));
    return 0;
}

/* ------------------------------ application ------------------------------ */

static rcl_publisher_t s_publisher;
static rcl_subscription_t s_subscription;
static rcl_service_t s_service;
static rcl_timer_t s_timer;
static rclc_parameter_server_t s_paramServer;
static std_msgs__msg__Int32 s_pubMsg;
static std_msgs__msg__Int32 s_subMsg;
static std_srvs__srv__Trigger_Request s_request;
static std_srvs__srv__Trigger_Response s_response;

static void timerCallback(rcl_timer_t* timer, int64_t lastCallTime)
{
    (void)lastCallTime;
    if (timer != NULL)
    {
        (void)rcl_publish(&s_publisher, &s_pubMsg, NULL);
        s_pubMsg.data++;
    }
}

static void subscriptionCallback(const void* msg)
{
    s_pubMsg.data = ((const std_msgs__msg__Int32*)msg)->data;
}

static void serviceCallback(const void* request, void* response)
{
    (void)request;
    std_srvs__srv__Trigger_Response* res = response;
    res->success = true;
    s_pubMsg.data = 0;
}

static bool parameterCallback(const Parameter* oldParam, const Parameter* newParam, void* context)
{
    (void)oldParam;
    (void)newParam;
    (void)context;
    return true;
}

static bool runMicroRos(void)
{
    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;
    rcl_node_t node;
    rclc_executor_t executor;

    bool ok = (rmw_uros_ping_agent(100, 1) == RMW_RET_OK)
        && (rclc_support_init(&support, 0, NULL, &allocator) == RCL_RET_OK)
        && (rclc_node_init_default(&node, "c2000_node", "", &support) == RCL_RET_OK)
        && (rclc_publisher_init_default(&s_publisher, &node,
                ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32), "c2000/counter") == RCL_RET_OK)
        && (rclc_subscription_init_default(&s_subscription, &node,
                ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32), "c2000/set_counter") == RCL_RET_OK)
        && (rclc_service_init_default(&s_service, &node,
                ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, Trigger), "c2000/reset") == RCL_RET_OK)
        && (rclc_timer_init_default2(&s_timer, &support, RCL_MS_TO_NS(100), timerCallback, true) == RCL_RET_OK)
        && (rclc_parameter_server_init_default(&s_paramServer, &node) == RCL_RET_OK)
        && (rclc_executor_init(&executor, &support.context, 3U + RCLC_EXECUTOR_PARAMETER_SERVER_HANDLES,
                               &allocator) == RCL_RET_OK)
        && (rclc_executor_add_timer(&executor, &s_timer) == RCL_RET_OK)
        && (rclc_executor_add_subscription(&executor, &s_subscription, &s_subMsg, subscriptionCallback,
                                           ON_NEW_DATA) == RCL_RET_OK)
        && (rclc_executor_add_service(&executor, &s_service, &s_request, &s_response,
                                      serviceCallback) == RCL_RET_OK)
        && (rclc_executor_add_parameter_server(&executor, &s_paramServer, parameterCallback) == RCL_RET_OK)
        && (rclc_add_parameter(&s_paramServer, "rate_hz", RCLC_PARAMETER_INT) == RCL_RET_OK)
        && (rmw_uros_sync_session(1000) == RMW_RET_OK);

    while (ok)
    {
        ok = (rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)) == RCL_RET_OK);
    }

    return ok;
}

static void microrosTask(void* arg)
{
    (void)arg;
    rmw_uros_set_custom_transport(true, NULL, stub_open, stub_close, stub_write, stub_read);
    for (;;)
    {
        if (!runMicroRos())
        {
            /* No agent: blink fast. */
            board_led_toggle(BOARD_LED_STATUS);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

int main(void)
{
    board_init();

    xTaskCreateStatic(microrosTask, "uros", MICROROS_TASK_STACK_WORDS, NULL,
                      tskIDLE_PRIORITY + 2U, s_microrosStack, &s_microrosTask);
    vTaskStartScheduler();

    for (;;)
    {
    }
}
