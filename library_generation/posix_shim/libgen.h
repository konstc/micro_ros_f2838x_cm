/*
 * Minimal <libgen.h> for the TI Arm Clang C library, which has none.
 * Implemented in port/src/libc_shims.c.
 */
#ifndef MICROROS_F2838X_POSIX_SHIM_LIBGEN_H
#define MICROROS_F2838X_POSIX_SHIM_LIBGEN_H

#ifdef __cplusplus
extern "C" {
#endif

char* basename(char* path);

#ifdef __cplusplus
}
#endif

#endif /* MICROROS_F2838X_POSIX_SHIM_LIBGEN_H */
