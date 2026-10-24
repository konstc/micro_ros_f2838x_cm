/*
 * micro-ROS port for the F2838x Connectivity Manager: public API.
 *
 * Call microros_f2838x_init() once from a FreeRTOS task (the scheduler must be
 * running), before rclc_support_init(). It registers the FreeRTOS heap as the
 * rcl allocator and the selected transport with rmw_microxrcedds.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MICROROS_F2838X_H
#define MICROROS_F2838X_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    MICROROS_F2838X_TRANSPORT_UDP,  /* Ethernet, lwIP, packet mode */
} microros_f2838x_transport_t;

typedef struct
{
    microros_f2838x_transport_t transport;

    struct
    {
        const char* ip;             /* board address, e.g. "192.168.10.2" */
        const char* netmask;        /* e.g. "255.255.255.0" */
        const char* gateway;        /* "0.0.0.0" if none */
        uint8_t mac[6];             /* use a locally administered address (0x02 in byte 0) */
        const char* agent_ip;       /* e.g. "192.168.10.1" */
        uint16_t agent_port;        /* micro-ROS agent UDP port, usually 8888 */
    } udp;
} microros_f2838x_cfg_t;

/* Session state, as maintained by the application (see examples/common). */
typedef enum
{
    MICROROS_F2838X_STATE_INIT = 0,
    MICROROS_F2838X_STATE_WAITING_AGENT,
    MICROROS_F2838X_STATE_AGENT_AVAILABLE,
    MICROROS_F2838X_STATE_CONNECTED,
    MICROROS_F2838X_STATE_DISCONNECTED,
    MICROROS_F2838X_STATE_ERROR,
} microros_f2838x_state_t;

/*
 * Run-time statistics. Updated by the port; the application updates state,
 * reconnects and uros_stack_free_min_words.
 */
typedef struct
{
    microros_f2838x_state_t state;
    uint32_t reconnects;

    uint32_t udp_rx_datagrams;
    uint32_t udp_rx_dropped;        /* receive buffer full or datagram larger than the MTU */
    uint32_t udp_tx_datagrams;
    uint32_t udp_tx_errors;

    uint32_t heap_in_use;           /* bytes allocated through the rcl allocator */
    uint32_t heap_in_use_peak;
    uint32_t heap_free_min;         /* FreeRTOS heap low-water mark, bytes */
    uint32_t allocs_while_locked;   /* allocations while microros_f2838x_lock_allocations(true) */

    uint32_t uros_stack_free_min_words;
    uint32_t net_stack_free_min_words;
} microros_f2838x_stats_t;

extern volatile microros_f2838x_stats_t microros_f2838x_stats;

/* Registers allocator and transport; for UDP also starts the network. */
bool microros_f2838x_init(const microros_f2838x_cfg_t* cfg);

/*
 * While locked, every allocation through the rcl allocator is counted in
 * stats.allocs_while_locked. Lock once all entities exist: the executor loop is
 * expected not to allocate.
 */
void microros_f2838x_lock_allocations(bool locked);

#ifdef __cplusplus
}
#endif

#endif /* MICROROS_F2838X_H */
