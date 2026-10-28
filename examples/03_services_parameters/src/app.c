/*
 * 03_services_parameters: a counter on /c2000/counter (std_msgs/Int32) whose
 * publish rate is the parameter rate_hz (rclc parameter server, 1 to 200 Hz,
 * 10 Hz at first), and the service /c2000/reset (std_srvs/Trigger), which
 * sets the counter back to 0. Network settings: examples/common/udp_config.c.
 *
 *   ros2 param set /c2000_node rate_hz 100
 *   ros2 service call /c2000/reset std_srvs/srv/Trigger
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "microros_app.h"

#include <string.h>

#include <rclc_parameter/rclc_parameter.h>
#include <std_msgs/msg/int32.h>
#include <std_srvs/srv/trigger.h>

#define RATE_PARAMETER                                                "rate_hz"
#define RATE_DEFAULT_HZ                                                    (10)
#define RATE_MIN_HZ                                                         (1)
#define RATE_MAX_HZ                                                       (200)

const char app_node_name[] = "c2000_node";

/* Timer, reset service and the parameter server. */
const unsigned app_executor_handles = 2U + RCLC_EXECUTOR_PARAMETER_SERVER_HANDLES;

static rcl_publisher_t s_publisher;
static rcl_timer_t s_timer;
static rcl_service_t s_resetService;
static rclc_parameter_server_t s_parameterServer;
static bool s_parameterServerReady;

static std_msgs__msg__Int32 s_message;
static std_srvs__srv__Trigger_Request s_resetRequest;
static std_srvs__srv__Trigger_Response s_resetResponse;
static char s_resetText[] = "counter reset";

/* The server pre-allocates its requests, responses and strings for
 * max_params parameters; with rclc's default of 4, creating the entities
 * took about 12 KB of heap. */
static const rclc_parameter_options_t s_parameterOptions = {
    .notify_changed_over_dds = true,
    .max_params = 1U,
    .allow_undeclared_parameters = false,
    .low_mem_mode = false,
};

/* Kept across reconnections, so that a new session keeps the last rate. */
static int64_t s_rateHz = RATE_DEFAULT_HZ;

static void timer_callback(MICROROS_F2838X_TIMER_CALLBACK_ARGS(timer, last_call_time))
{
    (void)last_call_time;

    if (timer != NULL)
    {
        (void)rcl_publish(&s_publisher, &s_message, NULL);
        s_message.data++;
    }
}

static void reset_callback(const void* request, void* response)
{
    (void)request;
    std_srvs__srv__Trigger_Response* reply = response;

    s_message.data = 0;
    reply->success = true;
    reply->message.data = s_resetText;
    reply->message.size = sizeof(s_resetText) - 1U;
    reply->message.capacity = sizeof(s_resetText);
}

/* Parameter server callback, also run by rclc_parameter_set_int(). Accepts
 * only rate_hz within range; deleting it is refused. */
static bool parameter_changed(const Parameter* old_param, const Parameter* new_param,
                              void* context)
{
    (void)old_param;
    (void)context;

    bool accepted = false;
    int64_t oldPeriod;

    if ((new_param != NULL) && (strcmp(new_param->name.data, RATE_PARAMETER) == 0) &&
        (new_param->value.type == RCLC_PARAMETER_INT) &&
        (new_param->value.integer_value >= RATE_MIN_HZ) &&
        (new_param->value.integer_value <= RATE_MAX_HZ) &&
        (rcl_timer_exchange_period(&s_timer, RCL_S_TO_NS(1) / new_param->value.integer_value,
                                   &oldPeriod) == RCL_RET_OK))
    {
        s_rateHz = new_param->value.integer_value;
        accepted = true;
    }

    return accepted;
}

bool app_create(rcl_node_t* node, rclc_support_t* support, rclc_executor_t* executor)
{
    s_publisher = rcl_get_zero_initialized_publisher();
    s_timer = rcl_get_zero_initialized_timer();
    s_resetService = rcl_get_zero_initialized_service();

    bool ok = (rclc_publisher_init_default(&s_publisher, node,
                                           ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
                                           "c2000/counter") == RCL_RET_OK) &&
              (rclc_timer_init_default2(&s_timer, support, RCL_S_TO_NS(1) / s_rateHz,
                                        timer_callback, true) == RCL_RET_OK) &&
              (rclc_service_init_default(&s_resetService, node,
                                         ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, Trigger),
                                         "c2000/reset") == RCL_RET_OK) &&
              (rclc_parameter_server_init_with_option(&s_parameterServer, node,
                                                      &s_parameterOptions) == RCL_RET_OK);

    /* app_destroy() finishes the parameter server only if it was created. */
    s_parameterServerReady = ok;

    ok = ok &&
         (rclc_executor_add_timer(executor, &s_timer) == RCL_RET_OK) &&
         (rclc_executor_add_service(executor, &s_resetService, &s_resetRequest,
                                    &s_resetResponse, reset_callback) == RCL_RET_OK) &&
         (rclc_executor_add_parameter_server(executor, &s_parameterServer,
                                             parameter_changed) == RCL_RET_OK) &&
         (rclc_add_parameter(&s_parameterServer, RATE_PARAMETER,
                             RCLC_PARAMETER_INT) == RCL_RET_OK) &&
         (rclc_parameter_set_int(&s_parameterServer, RATE_PARAMETER, s_rateHz) == RCL_RET_OK) &&
         (rclc_add_parameter_constraint_integer(&s_parameterServer, RATE_PARAMETER,
                                                RATE_MIN_HZ, RATE_MAX_HZ, 1) == RCL_RET_OK) &&
         (rclc_add_parameter_description(&s_parameterServer, RATE_PARAMETER,
                                         "Publish rate of /c2000/counter in Hz",
                                         "1 to 200") == RCL_RET_OK);

    return ok;
}

void app_destroy(rcl_node_t* node)
{
    if (s_parameterServerReady)
    {
        (void)rclc_parameter_server_fini(&s_parameterServer, node);
        s_parameterServerReady = false;
    }

    (void)rcl_service_fini(&s_resetService, node);
    (void)rcl_timer_fini(&s_timer);
    (void)rcl_publisher_fini(&s_publisher, node);
}
