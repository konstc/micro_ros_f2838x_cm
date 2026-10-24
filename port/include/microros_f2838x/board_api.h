/*
 * Board interface of the micro-ROS F2838x port.
 *
 * The port layer (port/src) uses the board only through these functions;
 * each board implements them in boards/<board>/cm/. Besides the functions,
 * a board's linker command file must provide:
 *   .task_stacks  task stacks (the port's net task places its stack there)
 *   .lwip.*       lwIP memory, in RAM the Ethernet DMA can reach
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MICROROS_F2838X_BOARD_API_H
#define MICROROS_F2838X_BOARD_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct netif;

/* Monotonic time in microseconds since board_init(). */
uint64_t board_time_us(void);

/*
 * Ethernet, driven by the port's net task with the lwIP core locked:
 *  - board_eth_configure() before netif_add(): MAC address, and the callback
 *    the board calls from interrupt context when board_eth_service() has work;
 *  - board_eth_netif_init() is passed to netif_add(): brings up the Ethernet
 *    controller and fills in the netif;
 *  - board_eth_service() hands received frames to netif->input and recycles
 *    transmitted buffers.
 */
typedef void (*board_eth_event_cb_t)(void);

void board_eth_configure(const uint8_t mac[6], board_eth_event_cb_t event_cb);
int8_t board_eth_netif_init(struct netif* netif);   /* lwIP err_t (s8_t) */
void board_eth_service(struct netif* netif);

#ifdef __cplusplus
}
#endif

#endif /* MICROROS_F2838X_BOARD_API_H */
