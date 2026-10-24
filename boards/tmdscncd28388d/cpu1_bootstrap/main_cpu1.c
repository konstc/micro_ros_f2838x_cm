/*
 * CPU1 bootstrap for micro-ROS on Connectivity Manager (TMDSCNCD28388D)
 *
 * micro-ROS runs on CM, but CM cannot start on its own: CPU1 owns the
 * clock tree and the pin mux. This program
 *   1. sets up the clocks (Device_init: SYSCLK 200 MHz, CMCLK 125 MHz from
 *      AUXPLL) and Ethernet clock (100 MHz),
 *   2. routes the CM's pins: Ethernet MII + MDIO, PHY reset and power-down,
 *      CM UART0 on GPIO84/85, and hands LED D1 (GPIO31) over to the CM,
 *   3. boots the CM from flash sector 0,
 *   4. then blinks LED D2 (GPIO34) as a heartbeat.
 *
 * Pins are configured before CM is released, so the CM never sees an
 * unconfigured pin. CM always runs from flash: the micro-ROS stack does
 * not fit in the CM's RAM. Based on C2000Ware's cm_common_config_c28x example.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driverlib.h"
#include "device.h"

/* D1 (GPIO31), driven by CM */
#define LED_CM_GPIO                                      (DEVICE_GPIO_PIN_LED1)
/* D2 (GPIO34), driven by CPU1 */
#define LED_HEARTBEAT_GPIO                               (DEVICE_GPIO_PIN_LED2)

#define ENET_PHY_POWERDOWN_GPIO                                          (108U)
#define ENET_PHY_RESET_GPIO                                              (119U)

static void configureEthernetPins(void);
static void configureUartPins(void);
static void configureLeds(void);

void main(void)
{
    /* Clocks, flash wait states, watchdog off, peripheral clocks on.
     * Device_init() also sets CMCLK = AUXPLL / 1 = 125 MHz. */
    Device_init();

    /* Unlock the pins and enable the internal pull-ups. */
    Device_initGPIO();

    /* Ethernet clock: SYSPLL / 2 = 100 MHz (also the PTP reference). */
    SysCtl_setEnetClk(SYSCTL_ENETCLKOUT_DIV_2, SYSCTL_SOURCE_SYSPLL);

    configureEthernetPins();
    configureUartPins();
    configureLeds();

    /* Release CM. */
    Device_bootCM(BOOTMODE_BOOT_TO_FLASH_SECTOR0);

    /* Heartbeat: CPU1 is alive. */
    for (;;)
    {
        GPIO_togglePin(LED_HEARTBEAT_GPIO);
        DEVICE_DELAY_US(500000U);
    }
}

static void configureEthernetPins(void)
{
    /* MDIO */
    GPIO_setPinConfig(GPIO_105_ENET_MDIO_CLK);
    GPIO_setPinConfig(GPIO_106_ENET_MDIO_DATA);

    /* MII (controlCARD PHY is connected in MII mode) */
    GPIO_setPinConfig(GPIO_109_ENET_MII_CRS);
    GPIO_setPinConfig(GPIO_110_ENET_MII_COL);
    GPIO_setPinConfig(GPIO_75_ENET_MII_TX_DATA0);
    GPIO_setPinConfig(GPIO_122_ENET_MII_TX_DATA1);
    GPIO_setPinConfig(GPIO_123_ENET_MII_TX_DATA2);
    GPIO_setPinConfig(GPIO_124_ENET_MII_TX_DATA3);
    GPIO_setPinConfig(GPIO_118_ENET_MII_TX_EN);
    GPIO_setPinConfig(GPIO_114_ENET_MII_RX_DATA0);
    GPIO_setPinConfig(GPIO_115_ENET_MII_RX_DATA1);
    GPIO_setPinConfig(GPIO_116_ENET_MII_RX_DATA2);
    GPIO_setPinConfig(GPIO_117_ENET_MII_RX_DATA3);
    GPIO_setPinConfig(GPIO_113_ENET_MII_RX_ERR);
    GPIO_setPinConfig(GPIO_112_ENET_MII_RX_DV);
    GPIO_setPinConfig(GPIO_44_ENET_MII_TX_CLK);
    GPIO_setPinConfig(GPIO_111_ENET_MII_RX_CLK);

    /* Release the PHY control lines. */
    GPIO_setDirectionMode(ENET_PHY_POWERDOWN_GPIO, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(ENET_PHY_POWERDOWN_GPIO, GPIO_PIN_TYPE_PULLUP);
    GPIO_writePin(ENET_PHY_POWERDOWN_GPIO, 1U);

    GPIO_setDirectionMode(ENET_PHY_RESET_GPIO, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(ENET_PHY_RESET_GPIO, GPIO_PIN_TYPE_PULLUP);
    GPIO_writePin(ENET_PHY_RESET_GPIO, 1U);
}

static void configureUartPins(void)
{
    /* CM UART0: RX on GPIO85, TX on GPIO84.
     * Note: GPIO84 is also the factory-default boot-mode pin 0; the USB-UART
     * adapter must not pull it low at power-up. */
    GPIO_setPinConfig(GPIO_85_UARTA_RX);
    GPIO_setDirectionMode(85U, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(85U, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(85U, GPIO_QUAL_ASYNC);

    GPIO_setPinConfig(GPIO_84_UARTA_TX);
    GPIO_setDirectionMode(84U, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(84U, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(84U, GPIO_QUAL_ASYNC);
}

static void configureLeds(void)
{
    /* D1 belongs to CM (status LED of the micro-ROS application). */
    GPIO_setPinConfig(DEVICE_GPIO_CFG_LED1);
    GPIO_setPadConfig(LED_CM_GPIO, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(LED_CM_GPIO, GPIO_DIR_MODE_OUT);
    GPIO_writePin(LED_CM_GPIO, 1U);
    GPIO_setMasterCore(LED_CM_GPIO, GPIO_CORE_CM);

    /* D2 stays with CPU1 (heartbeat). */
    GPIO_setPinConfig(DEVICE_GPIO_CFG_LED2);
    GPIO_setPadConfig(LED_HEARTBEAT_GPIO, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(LED_HEARTBEAT_GPIO, GPIO_DIR_MODE_OUT);
    GPIO_writePin(LED_HEARTBEAT_GPIO, 1U);
}
