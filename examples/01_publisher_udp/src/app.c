/*
 * 01_publisher_udp: publishes a counter (std_msgs/Int32) on /c2000/counter
 * over Ethernet/UDP. Network settings: examples/common/udp_config.c.
 *
 *   ros2 topic hz /c2000/counter
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "microros_app.h"

#include <std_msgs/msg/int32.h>

#define PUBLISH_RATE_HZ                                                  (100U)

const char app_node_name[] = "c2000_node";
const unsigned app_executor_handles = 1U;

static rcl_publisher_t s_publisher;
static rcl_timer_t s_timer;
static std_msgs__msg__Int32 s_message;

static void timer_callback(rcl_timer_t* timer, int64_t last_call_time)
{
    (void)last_call_time;

    if (timer != NULL)
    {
        (void)rcl_publish(&s_publisher, &s_message, NULL);
        s_message.data++;
    }
}

bool app_create(rcl_node_t* node, rclc_support_t* support, rclc_executor_t* executor)
{
    s_publisher = rcl_get_zero_initialized_publisher();
    s_timer = rcl_get_zero_initialized_timer();

    return (rclc_publisher_init_default(&s_publisher, node,
                                        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
                                        "c2000/counter") == RCL_RET_OK) &&
           (rclc_timer_init_default2(&s_timer, support,
                                     RCL_MS_TO_NS(1000U / PUBLISH_RATE_HZ),
                                     timer_callback, true) == RCL_RET_OK) &&
           (rclc_executor_add_timer(executor, &s_timer) == RCL_RET_OK);
}

void app_destroy(rcl_node_t* node)
{
    (void)rcl_timer_fini(&s_timer);
    (void)rcl_publisher_fini(&s_publisher, node);
}
