/*
 * Microseconds to struct timespec (internal to the port).
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MICROROS_F2838X_CLOCK_CONV_H
#define MICROROS_F2838X_CLOCK_CONV_H

#include <stdint.h>
#include <time.h>

static inline void microros_f2838x_us_to_timespec(uint64_t us, struct timespec* tp)
{
    tp->tv_sec = (time_t)(us / 1000000U);
    tp->tv_nsec = (long)((us % 1000000U) * 1000U);
}

#endif /* MICROROS_F2838X_CLOCK_CONV_H */
