/*
 * micro-ROS transport: UDP over lwIP, packet mode.
 *
 * One datagram carries one XRCE message, so rmw_microxrcedds registers this
 * transport without framing. The receive callback runs in the net task: it
 * copies the datagram into a FreeRTOS message buffer and frees the pbuf at
 * once. The pbufs come from the Ethernet receive pool, of which the driver
 * keeps most in its DMA descriptors; holding them until the micro-ROS task
 * reads would starve reception. Sending runs in the micro-ROS task under the
 * lwIP lock.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include "transports.h"

#include "FreeRTOS.h"
#include "message_buffer.h"

#include "lwip/ip4_addr.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "microros_f2838x/microros_f2838x.h"
#include "microros_f2838x/microros_f2838x_config.h"
#include "microros_f2838x_build_info.h"
#include "net_lwip.h"

#define UDP_MTU              (MICROROS_F2838X_LIB_UCLIENT_CUSTOM_TRANSPORT_MTU)

static ip_addr_t s_agentIp;
static uint16_t s_agentPort;
static struct udp_pcb* s_pcb;

static StaticMessageBuffer_t s_rxBufferStruct;
static uint8_t s_rxBufferStorage[MICROROS_F2838X_UDP_RX_BUFFER_BYTES + 1U];
static MessageBufferHandle_t s_rxBuffer;
static uint8_t s_rxScratch[UDP_MTU];      /* used by the net task only */

bool udp_transport_configure(const char* agent_ip, uint16_t agent_port)
{
    bool ret = false;

    if (agent_ip != NULL)
    {
        if (ipaddr_aton(agent_ip, &s_agentIp) && (agent_port != 0U))
        {
            s_agentPort = agent_port;

            if (s_rxBuffer == NULL)
            {
                s_rxBuffer = xMessageBufferCreateStatic(sizeof(s_rxBufferStorage), s_rxBufferStorage,
                                                        &s_rxBufferStruct);
            }
            ret = s_rxBuffer != NULL;
        }
    }

    return ret;
}

/* lwIP receive callback: net task, lwIP core locked. */
static void udp_received(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                         const ip_addr_t* addr, u16_t port)
{
    (void)arg;
    (void)pcb;
    (void)addr;
    (void)port;

    if ((p->tot_len <= sizeof(s_rxScratch)) &&
        (pbuf_copy_partial(p, s_rxScratch, p->tot_len, 0U) == p->tot_len) &&
        (xMessageBufferSend(s_rxBuffer, s_rxScratch, p->tot_len, 0U) == p->tot_len))
    {
        microros_f2838x_stats.udp_rx_datagrams++;
    }
    else
    {
        microros_f2838x_stats.udp_rx_dropped++;
    }

    pbuf_free(p);
}

bool udp_transport_open(struct uxrCustomTransport* transport)
{
    (void)transport;

    net_lock();
    s_pcb = udp_new();
    bool ok = (s_pcb != NULL) &&
              (udp_bind(s_pcb, IP_ADDR_ANY, 0U) == ERR_OK) &&
              (udp_connect(s_pcb, &s_agentIp, s_agentPort) == ERR_OK);
    if (ok)
    {
        udp_recv(s_pcb, udp_received, NULL);
    }
    else if (s_pcb != NULL)
    {
        udp_remove(s_pcb);
        s_pcb = NULL;
    }
    net_unlock();

    (void)xMessageBufferReset(s_rxBuffer);
    return ok;
}

bool udp_transport_close(struct uxrCustomTransport* transport)
{
    (void)transport;

    net_lock();
    if (s_pcb != NULL)
    {
        udp_remove(s_pcb);
        s_pcb = NULL;
    }
    net_unlock();

    return true;
}

size_t udp_transport_write(struct uxrCustomTransport* transport, const uint8_t* buf,
                           size_t len, uint8_t* err)
{
    (void)transport;
    err_t result = ERR_MEM;
    size_t written = 0U;

    net_lock();
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
    if (p != NULL)
    {
        (void)pbuf_take(p, buf, (u16_t)len);
        result = (s_pcb != NULL) ? udp_send(s_pcb, p) : ERR_CONN;
        pbuf_free(p);
    }
    net_unlock();

    if (result == ERR_OK)
    {
        microros_f2838x_stats.udp_tx_datagrams++;
        written = len;
    }
    else
    {
        microros_f2838x_stats.udp_tx_errors++;
        *err = 1U;
    }

    return written;
}

size_t udp_transport_read(struct uxrCustomTransport* transport, uint8_t* buf,
                          size_t len, int timeout, uint8_t* err)
{
    (void)transport;
    (void)err;

    TickType_t ticks = (timeout > 0) ? pdMS_TO_TICKS((uint32_t)timeout) : 0U;
    return xMessageBufferReceive(s_rxBuffer, buf, len, ticks);
}
