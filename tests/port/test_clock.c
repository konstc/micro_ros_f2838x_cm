/*
 * Host tests for the microsecond to timespec conversion of port/src/clock.c.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <stdio.h>

#include "clock_conv.h"

static int s_failures;

static void check(uint64_t us, long long sec, long nsec)
{
    struct timespec ts;
    microros_f2838x_us_to_timespec(us, &ts);
    if (((long long)ts.tv_sec != sec) || (ts.tv_nsec != nsec))
    {
        printf("%llu us -> %lld s %ld ns, expected %lld s %ld ns\n",
               (unsigned long long)us, (long long)ts.tv_sec, ts.tv_nsec, sec, nsec);
        s_failures++;
    }
}

int main(void)
{
    check(0U, 0, 0L);
    check(1U, 0, 1000L);
    check(999999U, 0, 999999000L);
    check(1000000U, 1, 0L);
    check(4294967295ULL, 4294, 967295000L);         /* 32-bit timer wrap */
    check(4294967296ULL, 4294, 967296000L);
    check(10ULL * 365 * 24 * 3600 * 1000000ULL + 5U, 10LL * 365 * 24 * 3600, 5000L);

    printf("test_clock: %s\n", s_failures ? "FAILED" : "passed");
    return s_failures ? 1 : 0;
}
