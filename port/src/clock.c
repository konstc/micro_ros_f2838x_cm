/*
 * clock_gettime() for micro-ROS (declared in posix_shim/time.h).
 *
 * Both clocks are the board's monotonic microsecond time since boot.
 * CLOCK_REALTIME must not jump: the Micro XRCE-DDS client measures its
 * timeouts with it. Wall-clock time comes from the agent instead: after
 * rmw_uros_sync_session(), rmw_uros_epoch_millis()/rmw_uros_epoch_nanos()
 * return it.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <time.h>

#include "clock_conv.h"
#include "microros_f2838x/board_api.h"

int clock_gettime(clockid_t clock_id, struct timespec* tp)
{
    int ret = -1;

    if ((tp == NULL) || ((clock_id != CLOCK_MONOTONIC) && (clock_id != CLOCK_REALTIME)))
    {
        errno = EINVAL;
    }
    else
    {
        microros_f2838x_us_to_timespec(board_time_us(), tp);
        ret = 0;
    }

    return ret;
}
