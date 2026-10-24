/*
 * POSIX additions to the TI Arm Clang C library <string.h>.
 */
#ifndef MICROROS_F2838X_POSIX_SHIM_STRING_H
#define MICROROS_F2838X_POSIX_SHIM_STRING_H

#include_next <string.h>

#ifdef __cplusplus
extern "C" {
#endif

int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, size_t n);
int strerror_r(int errnum, char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* MICROROS_F2838X_POSIX_SHIM_STRING_H */
