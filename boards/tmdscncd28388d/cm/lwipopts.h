/*
 * lwIP configuration for micro-ROS on the TMDSCNCD28388D (CM): UDP to the
 * micro-ROS agent, plus ARP and ICMP (ping). No TCP, no sockets/netconn.
 *
 * NO_SYS=1: the port's net task owns lwIP (port/src/net_lwip.c).
 * Values required by C2000Ware's f2838xif.c driver are marked "driver".
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* System */
#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            1       /* driver */
#define LWIP_TIMERS                     1
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0
#define LWIP_NETIF_API                  0

/* Memory */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (4U * 1024U)    /* PBUF_RAM: outgoing datagrams */
#define MEMP_NUM_PBUF                   8
#define MEMP_NUM_UDP_PCB                2
#define MEMP_NUM_RAW_PCB                0
#define PBUF_POOL_SIZE                  10      /* 8 receive descriptors + 2 in flight */
#define PBUF_POOL_BUFSIZE               1538    /* driver: one full frame per buffer */
#define PBUF_LINK_HLEN                  16      /* driver */
#define ETH_PAD_SIZE                    0       /* driver */

#define LWIP_DECLARE_MEMORY_ALIGNED(variable_name, size) \
    u8_t variable_name[LWIP_MEM_ALIGN_BUFFER(size)]      \
    __attribute__((section(".lwip." #variable_name), aligned(MEM_ALIGNMENT)))

/* IPv4 */
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_ARP                        1
#define ARP_TABLE_SIZE                  4
#define ARP_QUEUEING                    0
#define IP_REASSEMBLY                   0
#define IP_FRAG                         0
#define LWIP_ICMP                       1
#define LWIP_RAW                        0
#define LWIP_DHCP                       0
#define LWIP_AUTOIP                     0
#define LWIP_IGMP                       0
#define LWIP_DNS                        0

/* Transport */
#define LWIP_UDP                        1
#define LWIP_TCP                        0

/* Interface */
#define LWIP_NETIF_HOSTNAME             0
#define LWIP_NETIF_STATUS_CALLBACK      0
#define LWIP_NETIF_LINK_CALLBACK        0

/* Statistics: inspect lwip_stats in the debugger. */
#define LWIP_STATS                      1
#define LWIP_STATS_DISPLAY              0

#endif /* LWIPOPTS_H */
