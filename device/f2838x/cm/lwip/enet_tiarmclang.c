/*
 * Builds C2000Ware's Ethernet driver helpers for lwIP with tiarmclang.
 * enet.c declares its ISR with the legacy "interrupt" keyword; Cortex-M
 * handlers are plain functions, so the keyword is undefined for this file.
 *
 * Needs the include path <lwip> (the directory that contains driver/).
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "driverlib_cm_tiarmclang.h"

#pragma clang diagnostic ignored "-Wunused-variable"     /* in TI's code */
#define interrupt
#include "driver/enet.c"
#undef interrupt
