/*
 * Ethernet for the TMDSCNCD28388D controlCARD (CM side): the board part of
 * the port's board interface (board_eth_*).
 *
 * Controller set-up, the packet-buffer callbacks and the interrupt handling
 * follow C2000Ware's enet_lwip_udp example;
 * lwIP netif glue is C2000Ware's f2838xif.c, built in device/f2838x/cm/lwip/.
 * Differences: the callbacks call f2838xif directly (C2000Ware's lwiplib.c
 * is not used), and the interrupts only notify the port's net task, which
 * drains the queues in task context.
 *
 * CPU1 (cpu1_bootstrap) sets the Ethernet clock and the MII pin mux and
 * releases the PHY.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>

#include "driverlib_cm_tiarmclang.h"

#include "board.h"

#include "lwip/netif.h"
#include "netif/f2838xif.h"

#define interrupt
#include "driver/enet.h"
#undef interrupt

#include "microros_f2838x/board_api.h"

#define PENDING_RX                                                       (0x1U)
#define PENDING_TX                                                       (0x2U)

static struct netif* s_netif;
static board_eth_event_cb_t s_eventCb;
static uint8_t s_mac[6];
static volatile uint32_t s_pending;

/* Defined in f2838xif.c, which transmits through it: the controller handle
 * must be stored there (TI's examples define the same global themselves). */
extern Ethernet_Handle emac_handle;

/* --------------------------- driver callbacks ---------------------------- */

/* Free receive buffer for a DMA descriptor (a PBUF_POOL pbuf). */
static Ethernet_Pkt_Desc* getPacketBuffer(void)
{
    return f2838xif_getFreeRxPacket();
}

/* Received frame: f2838xif passes it to netif->input and returns a fresh buffer. */
static Ethernet_Pkt_Desc* receivePacket(Ethernet_Handle handle, Ethernet_Pkt_Desc* packet)
{
    (void)handle;
    return f2838xif_interrupt(s_netif, packet);
}

/* Transmitted frame: f2838xif frees its pbufs. */
static void releaseTxPacket(Ethernet_Handle handle, Ethernet_Pkt_Desc* packet)
{
    (void)handle;
    (void)f2838xif_interrupt(s_netif, packet);
}

/* ------------------------------ interrupts ------------------------------  */

static void notify(uint32_t what)
{
    s_pending |= what;

    if (s_eventCb != NULL)
    {
        s_eventCb();
    }
}

static void transmitIsr(void)
{
    Ethernet_clearDMAChannelInterrupt(Ethernet_device_struct.baseAddresses.enet_base,
                                      ETHERNET_DMA_CHANNEL_NUM_0, ETHERNET_DMA_CH0_STATUS_TI);
    notify(PENDING_TX);
}

static void receiveIsr(void)
{
    Ethernet_clearDMAChannelInterrupt(Ethernet_device_struct.baseAddresses.enet_base,
                                      ETHERNET_DMA_CHANNEL_NUM_0, ETHERNET_DMA_CH0_STATUS_RI);
    notify(PENDING_RX);
}

/* ------------------------------ controller ------------------------------- */

static void coreInterruptDisable(void)
{
    (void)Interrupt_disableInProcessor();
}

static void coreInterruptEnable(void)
{
    (void)Interrupt_enableInProcessor();
}

static void initController(const uint8_t mac[6])
{
    Ethernet_InitInterfaceConfig interfaceConfig;
    Ethernet_InitConfig* initConfig;

    interfaceConfig.ssbase = EMAC_SS_BASE;
    interfaceConfig.enet_base = EMAC_BASE;
    interfaceConfig.phyMode = ETHERNET_SS_PHY_INTF_SEL_MII;
    interfaceConfig.ptrPlatformInterruptDisable = &Platform_disableInterrupt;
    interfaceConfig.ptrPlatformInterruptEnable = &Platform_enableInterrupt;
    interfaceConfig.ptrPlatformPeripheralEnable = &Platform_enablePeripheral;
    interfaceConfig.ptrPlatformPeripheralReset = &Platform_resetPeripheral;
    interfaceConfig.ptrCoreInterruptDisable = &coreInterruptDisable;
    interfaceConfig.ptrCoreInterruptEnable = &coreInterruptEnable;
    interfaceConfig.peripheralNum = SYSCTL_PERIPH_CLK_ENET;
    interfaceConfig.interruptNum[0] = INT_EMAC;
    interfaceConfig.interruptNum[1] = INT_EMAC_TX0;
    interfaceConfig.interruptNum[2] = INT_EMAC_TX1;
    interfaceConfig.interruptNum[3] = INT_EMAC_RX0;
    interfaceConfig.interruptNum[4] = INT_EMAC_RX1;

    initConfig = Ethernet_initInterface(interfaceConfig);
    Ethernet_getInitConfig(initConfig);
    initConfig->dmaMode.InterruptMode = ETHERNET_DMA_MODE_INTM_MODE2;
    initConfig->pfcbRxPacket = &receivePacket;
    initConfig->pfcbGetPacket = &getPacketBuffer;
    initConfig->pfcbFreePacket = &releaseTxPacket;
    initConfig->numChannels = 1U;

    /* Fills the receive descriptors through getPacketBuffer(). The
     * application handle is not used. */
    Ethernet_getHandle((Ethernet_Handle)1, initConfig, &emac_handle);

    /* Transmit-buffer-unavailable and normal-summary interrupts are not used. */
    Ethernet_disableDmaInterrupt(Ethernet_device_struct.baseAddresses.enet_base, 0U,
                                 ETHERNET_DMA_CH0_INTERRUPT_ENABLE_TBUE |
                                 ETHERNET_DMA_CH0_INTERRUPT_ENABLE_NIE);

    /* Receive FIFO overflow is handled by Ethernet_genericISRCustom (enet.c). */
    Ethernet_enableMTLInterrupt(Ethernet_device_struct.baseAddresses.enet_base, 0U,
                                ETHERNET_MTL_Q0_INTERRUPT_CONTROL_STATUS_RXOIE);

    /* MAC management counter interrupts are not used. */
    HWREG(Ethernet_device_struct.baseAddresses.enet_base + ETHERNET_O_MMC_RX_INTERRUPT_MASK) = 0xFFFFFFFFU;
    HWREG(Ethernet_device_struct.baseAddresses.enet_base + ETHERNET_O_MMC_IPC_RX_INTERRUPT_MASK) = 0xFFFFFFFFU;
    HWREG(Ethernet_device_struct.baseAddresses.enet_base + ETHERNET_O_MMC_TX_INTERRUPT_MASK) = 0xFFFFFFFFU;

    Interrupt_registerHandler(INT_EMAC_TX0, transmitIsr);
    Interrupt_registerHandler(INT_EMAC_RX0, receiveIsr);
    Interrupt_registerHandler(INT_EMAC, Ethernet_genericISRCustom);
    Interrupt_setPriority(INT_EMAC_TX0, BOARD_IRQ_PRIORITY_ETH);
    Interrupt_setPriority(INT_EMAC_RX0, BOARD_IRQ_PRIORITY_ETH);
    Interrupt_setPriority(INT_EMAC, BOARD_IRQ_PRIORITY_ETH);

    /* Unicast MAC address; Ethernet_getHandle() programmed a dummy one. */
    uint32_t macLow = (uint32_t)mac[0] | ((uint32_t)mac[1] << 8) |
                      ((uint32_t)mac[2] << 16) | ((uint32_t)mac[3] << 24);
    uint32_t macHigh = (uint32_t)mac[4] | ((uint32_t)mac[5] << 8);
    Ethernet_setMACAddr(EMAC_BASE, 0U, macHigh, macLow, ETHERNET_CHANNEL_0);

    Ethernet_clearMACConfigurationCustom(Ethernet_device_struct.baseAddresses.enet_base,
                                         ETHERNET_MAC_CONFIGURATION_RE);
    Ethernet_setMACConfigurationCustom(Ethernet_device_struct.baseAddresses.enet_base,
                                       ETHERNET_MAC_CONFIGURATION_RE);

    Interrupt_enable(INT_EMAC_TX0);
    Interrupt_enable(INT_EMAC_RX0);
    Interrupt_enable(INT_EMAC);
}

/* ---------------------------- board interface ---------------------------- */

void board_eth_configure(const uint8_t mac[6], board_eth_event_cb_t event_cb)
{
    memcpy(s_mac, mac, sizeof(s_mac));
    s_eventCb = event_cb;
}

int8_t board_eth_netif_init(struct netif* netif)
{
    s_netif = netif;
    f2838xif_initQueue();           /* before the controller asks for receive buffers */
    initController(s_mac);
    return f2838xif_init(netif);    /* reads the MAC address back from the controller */
}

void board_eth_service(struct netif* netif)
{
    (void)netif;

    bool wasDisabled = Interrupt_disableInProcessor();
    uint32_t pending = s_pending;
    s_pending = 0U;

    if (!wasDisabled)
    {
        (void)Interrupt_enableInProcessor();
    }

    if ((pending & PENDING_RX) != 0U)
    {
        Ethernet_removePacketsFromRxQueue(
            (Ethernet_DescCh*)&Ethernet_device_struct.dmaObj.rxDma[ETHERNET_DMA_CHANNEL_NUM_0],
            ETHERNET_COMPLETION_NORMAL);
    }

    if ((pending & PENDING_TX) != 0U)
    {
        Ethernet_removePacketsFromTxQueue(
            (Ethernet_DescCh*)&Ethernet_device_struct.dmaObj.txDma[ETHERNET_DMA_CHANNEL_NUM_0],
            ETHERNET_COMPLETION_NORMAL);
    }
}
