/*
 * lwIP network task (internal to the port).
 *
 * lwIP runs with NO_SYS=1: one FreeRTOS task, the net task, owns the stack.
 * It processes Ethernet events and lwIP timers with the core locked. Any
 * other task that calls lwIP must hold the lock (net_lock()/net_unlock()).
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MICROROS_F2838X_NET_LWIP_H
#define MICROROS_F2838X_NET_LWIP_H

#include <stdbool.h>
#include <stdint.h>

/* Starts the net task and waits until the interface is up. */
bool net_start(const char* ip, const char* netmask, const char* gateway, const uint8_t mac[6]);

void net_lock(void);
void net_unlock(void);

#endif /* MICROROS_F2838X_NET_LWIP_H */
