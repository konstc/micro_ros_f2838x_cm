/*
 * Minimal <unistd.h> for the TI Arm Clang C library, which has none.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MICROROS_F2838X_POSIX_SHIM_UNISTD_H
#define MICROROS_F2838X_POSIX_SHIM_UNISTD_H

#include <sys/types.h>

/* clock_gettime() is available (rcutils/src/time_unix.c checks this). */
#ifndef _POSIX_TIMERS
#define _POSIX_TIMERS 200809L
#endif

#ifdef __cplusplus
extern "C" {
#endif

pid_t getpid(void);
int isatty(int fd);
int usleep(useconds_t usec);

#ifdef __cplusplus
}
#endif

#endif /* MICROROS_F2838X_POSIX_SHIM_UNISTD_H */
