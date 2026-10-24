/*
 * 02_ping_pong_udp: answers every std_msgs/Header on /c2000/ping with one on
 * /c2000/pong, best effort both ways, for tools/rtt_histogram.py.
 *
 * The pong carries the ping's frame_id (the PC's sequence number) and, as its
 * stamp, the board's time synchronized with the agent (rmw_uros_sync_session()
 * at connection and every SYNC_PERIOD_MS), so the script can also report the
 * clock offset between board and PC. The stamp is 0 until the first
 * synchronization succeeds. Network settings: examples/common/udp_config.c.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "microros_app.h"

#include <string.h>

#include <rmw_microros/rmw_microros.h>
#include <std_msgs/msg/header.h>

/* bytes, including the terminating NUL */
#define FRAME_ID_CAPACITY                                                 (32U)

#define SYNC_PERIOD_MS                                                 (10000U)
#define SYNC_TIMEOUT_MS                                                   (100)

const char app_node_name[] = "c2000_node";
const unsigned app_executor_handles = 2U;

static rcl_subscription_t s_pingSubscription;
static rcl_publisher_t s_pongPublisher;
static rcl_timer_t s_syncTimer;

/* The subscription deserializes into s_ping; both frame_ids need storage. */
static std_msgs__msg__Header s_ping;
static std_msgs__msg__Header s_pong;
static char s_pingFrameId[FRAME_ID_CAPACITY];
static char s_pongFrameId[FRAME_ID_CAPACITY];

static void ping_received(const void* message)
{
    const std_msgs__msg__Header* ping = message;

    size_t length = ping->frame_id.size;
    if (length >= FRAME_ID_CAPACITY)
    {
        length = FRAME_ID_CAPACITY - 1U;
    }
    memcpy(s_pong.frame_id.data, ping->frame_id.data, length);
    s_pong.frame_id.data[length] = '\0';
    s_pong.frame_id.size = length;

    int64_t nowNs = rmw_uros_epoch_synchronized() ? rmw_uros_epoch_nanos() : 0;
    s_pong.stamp.sec = (int32_t)(nowNs / 1000000000LL);
    s_pong.stamp.nanosec = (uint32_t)(nowNs % 1000000000LL);

    (void)rcl_publish(&s_pongPublisher, &s_pong, NULL);
}

static void sync_callback(rcl_timer_t* timer, int64_t last_call_time)
{
    (void)last_call_time;

    if (timer != NULL)
    {
        (void)rmw_uros_sync_session(SYNC_TIMEOUT_MS);
    }
}

static void init_frame_id(rosidl_runtime_c__String* frameId, char* storage)
{
    storage[0] = '\0';
    frameId->data = storage;
    frameId->size = 0U;
    frameId->capacity = FRAME_ID_CAPACITY;
}

bool app_create(rcl_node_t* node, rclc_support_t* support, rclc_executor_t* executor)
{
    s_pingSubscription = rcl_get_zero_initialized_subscription();
    s_pongPublisher = rcl_get_zero_initialized_publisher();
    s_syncTimer = rcl_get_zero_initialized_timer();
    init_frame_id(&s_ping.frame_id, s_pingFrameId);
    init_frame_id(&s_pong.frame_id, s_pongFrameId);

    /* First synchronization; the timer repeats it. */
    (void)rmw_uros_sync_session(SYNC_TIMEOUT_MS);

    return (rclc_subscription_init_best_effort(&s_pingSubscription, node,
                                               ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Header),
                                               "c2000/ping") == RCL_RET_OK) &&
           (rclc_publisher_init_best_effort(&s_pongPublisher, node,
                                            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Header),
                                            "c2000/pong") == RCL_RET_OK) &&
           (rclc_timer_init_default2(&s_syncTimer, support, RCL_MS_TO_NS(SYNC_PERIOD_MS),
                                     sync_callback, true) == RCL_RET_OK) &&
           (rclc_executor_add_subscription(executor, &s_pingSubscription, &s_ping,
                                           ping_received, ON_NEW_DATA) == RCL_RET_OK) &&
           (rclc_executor_add_timer(executor, &s_syncTimer) == RCL_RET_OK);
}

void app_destroy(rcl_node_t* node)
{
    (void)rcl_timer_fini(&s_syncTimer);
    (void)rcl_publisher_fini(&s_pongPublisher, node);
    (void)rcl_subscription_fini(&s_pingSubscription, node);
}
