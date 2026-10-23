/*
 * Builds C2000Ware's lwIP netif glue for the F2838x Ethernet controller
 * with tiarmclang.
 *
 * Needs the include path <lwip>/ports/C2000.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "driverlib_cm_tiarmclang.h"

#pragma clang diagnostic ignored "-Wunused-variable"     /* in TI's code */
#include "netif/f2838xif.c"
