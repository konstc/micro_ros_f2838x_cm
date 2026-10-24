/*
 * POSIX functions that micro-ROS uses and the TI Arm Clang C library lacks.
 * Declared in library_generation/posix_shim/.
 *
 * CM has no processes, files or environment; these implementations stub
 * the answers rcutils expects in that situation.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"

pid_t getpid(void)
{
    return 1;
}

int isatty(int fd)
{
    (void)fd;
    errno = ENOTTY;
    return 0;
}

int fileno(FILE* stream)
{
    (void)stream;
    errno = EBADF;
    return -1;
}

int usleep(useconds_t usec)
{
    /* Rounds up to whole ticks; rclc_sleep_ms() is the only user. */
    TickType_t ticks = (TickType_t)(((uint64_t)usec * configTICK_RATE_HZ + 999999U) / 1000000U);
    vTaskDelay(ticks);
    return 0;
}

char* basename(char* path)
{
    static char s_dot[] = ".";      /* POSIX allows static storage for the result */
    char* ret = s_dot;

    if ((path != NULL) && (*path != '\0'))
    {
        char* slash = strrchr(path, '/');
        ret = (slash != NULL) ? (slash + 1) : path;
    }

    return ret;
}

int setenv(const char* name, const char* value, int overwrite)
{
    (void)name;
    (void)value;
    (void)overwrite;

    errno = ENOSYS;
    return -1;
}

int unsetenv(const char* name)
{
    (void)name;
    errno = ENOSYS;
    return -1;
}

int strerror_r(int errnum, char* buf, size_t buflen)
{
    int ret = ERANGE;

    if ((buf != NULL) && (buflen != 0U))
    {
        const char* msg = strerror(errnum);
        size_t len = strlen(msg);

        if (len < buflen)
        {
            memcpy(buf, msg, len + 1U);
            ret = 0;
        }
        else
        {
            /* Truncated: still NUL-terminated, but reported as ERANGE. */
            memcpy(buf, msg, buflen - 1U);
            buf[buflen - 1U] = '\0';
        }
    }

    return ret;
}
