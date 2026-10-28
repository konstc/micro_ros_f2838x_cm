/*
 * rcl API differences between the supported distributions, so that one
 * application compiles against any of their libraries.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MICROROS_F2838X_RCL_COMPAT_H
#define MICROROS_F2838X_RCL_COMPAT_H

#include <stdint.h>

#include <rcl/timer.h>
#include <rcl_action/version.h>

/*
 * Parameters of a timer callback (rcl_timer_callback_t). Since Lyrical, rcl
 * passes the timer's user data as a third argument. Usage:
 *   static void on_timer(MICROROS_F2838X_TIMER_CALLBACK_ARGS(timer, last_call_time))
 */
#if RCL_ACTION_VERSION_MAJOR >= 10
#define MICROROS_F2838X_TIMER_CALLBACK_ARGS(timer, last_call_time) \
    rcl_timer_t* timer, int64_t last_call_time, uintptr_t timer_user_data __attribute__((unused))
#else
#define MICROROS_F2838X_TIMER_CALLBACK_ARGS(timer, last_call_time) \
    rcl_timer_t* timer, int64_t last_call_time
#endif

#endif /* MICROROS_F2838X_RCL_COMPAT_H */
