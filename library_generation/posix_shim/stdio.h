/*
 * POSIX additions to the TI Arm Clang C library <stdio.h>.
 * Implemented in port/src/libc_shims.c.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MICROROS_F2838X_POSIX_SHIM_STDIO_H
#define MICROROS_F2838X_POSIX_SHIM_STDIO_H

#include_next <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int fileno(FILE* stream);

#ifdef __cplusplus
}
#endif

#endif /* MICROROS_F2838X_POSIX_SHIM_STDIO_H */
