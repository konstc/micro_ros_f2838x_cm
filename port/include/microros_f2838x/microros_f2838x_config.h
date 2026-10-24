/*
 * Build-time settings of the micro-ROS F2838x port. Override any of them with
 * a compiler define (-D) in the project.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MICROROS_F2838X_CONFIG_H
#define MICROROS_F2838X_CONFIG_H

#include "FreeRTOSConfig.h"

/* Net task (owner of the lwIP core). Runs above the micro-ROS task. */
#ifndef MICROROS_F2838X_NET_TASK_STACK_WORDS
#define MICROROS_F2838X_NET_TASK_STACK_WORDS                             (512U)
#endif
#ifndef MICROROS_F2838X_NET_TASK_PRIORITY
#define MICROROS_F2838X_NET_TASK_PRIORITY           (configMAX_PRIORITIES - 2U)
#endif

/* Longest the net task sleeps between lwIP timer checks. */
#ifndef MICROROS_F2838X_NET_MAX_SLEEP_MS
#define MICROROS_F2838X_NET_MAX_SLEEP_MS                                 (100U)
#endif

/* Received UDP datagrams waiting for the micro-ROS task, in bytes. */
#ifndef MICROROS_F2838X_UDP_RX_BUFFER_BYTES
#define MICROROS_F2838X_UDP_RX_BUFFER_BYTES                             (2048U)
#endif

#endif /* MICROROS_F2838X_CONFIG_H */
