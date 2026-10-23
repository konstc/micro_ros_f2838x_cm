/*
 * POSIX additions to the TI Arm Clang C library <stdlib.h>.
 * Implemented in port/src/libc_shims.c.
 */
#ifndef MICROROS_F2838X_POSIX_SHIM_STDLIB_H
#define MICROROS_F2838X_POSIX_SHIM_STDLIB_H

#include_next <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* MICROROS_F2838X_POSIX_SHIM_STDLIB_H */
