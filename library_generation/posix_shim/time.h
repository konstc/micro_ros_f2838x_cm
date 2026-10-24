/*
 * POSIX additions to the TI Arm Clang C library <time.h>.
 */
#ifndef MICROROS_F2838X_POSIX_SHIM_TIME_H
#define MICROROS_F2838X_POSIX_SHIM_TIME_H

#include_next <time.h>
#include <sys/types.h>
#include <sys/timespec.h>

#if !defined(__TI_TIME_USES_64) || !__TI_TIME_USES_64
#error "libmicroros.a is built with 64-bit time_t: define __TI_TIME_USES_64=1 for the whole project"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME                                           ((clockid_t)0)
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC                                          ((clockid_t)1)
#endif

int clock_gettime(clockid_t clock_id, struct timespec* tp);

#ifdef __cplusplus
}
#endif

#endif /* MICROROS_F2838X_POSIX_SHIM_TIME_H */
