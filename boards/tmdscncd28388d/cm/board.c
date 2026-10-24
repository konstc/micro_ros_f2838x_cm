/*
 * Board support for the TMDSCNCD28388D controlCARD, CM side.
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>

#include "driverlib_cm_tiarmclang.h"

#include "board.h"

#include "FreeRTOS.h"

#include "microros_f2838x/board_api.h"

/* Time base: CPU timer 1 counts down at 1 MHz over 32 bits; its wrap interrupt
 * (every 71.6 minutes) extends the count to 64 bits. */
#define TIME_TIMER_BASE                                        (CPUTIMER1_BASE)
#define TIME_TIMER_INT                                             (INT_TIMER1)
#define TIME_TIMER_PRESCALER                    ((CM_CLK_FREQ / 1000000U) - 1U)

/* FreeRTOS heap (configAPPLICATION_ALLOCATED_HEAP), in its own section so that
 * the linker command file controls its placement and the map file shows it. */
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".freertos_heap"), aligned(8)));

static volatile uint32_t s_timePeriods;

static const uint32_t s_ledPins[] = {
    [BOARD_LED_STATUS] = DEVICE_GPIO_PIN_LED1,
};

static void timeWrapIsr(void)
{
    CPUTimer_clearOverflowFlag(TIME_TIMER_BASE);
    s_timePeriods++;
}

static void initTimeBase(void)
{
    CPUTimer_stopTimer(TIME_TIMER_BASE);
    CPUTimer_setPeriod(TIME_TIMER_BASE, 0xFFFFFFFFU);
    CPUTimer_setPreScaler(TIME_TIMER_BASE, (uint16_t)TIME_TIMER_PRESCALER);
    CPUTimer_setEmulationMode(TIME_TIMER_BASE, CPUTIMER_EMULATIONMODE_RUNFREE);
    CPUTimer_reloadTimerCounter(TIME_TIMER_BASE);
    CPUTimer_clearOverflowFlag(TIME_TIMER_BASE);
    CPUTimer_enableInterrupt(TIME_TIMER_BASE);

    Interrupt_registerHandler(TIME_TIMER_INT, timeWrapIsr);
    Interrupt_setPriority(TIME_TIMER_INT, BOARD_IRQ_PRIORITY_TIME);
    Interrupt_enable(TIME_TIMER_INT);

    CPUTimer_startTimer(TIME_TIMER_BASE);
}

uint64_t board_time_us(void)
{
    bool wasDisabled = Interrupt_disableInProcessor();

    uint32_t periods = s_timePeriods;
    uint32_t count = CPUTimer_getTimerCount(TIME_TIMER_BASE);

    if (CPUTimer_getTimerOverflowStatus(TIME_TIMER_BASE))
    {
        /* Wrapped, the interrupt has not run yet: count again after the wrap. */
        count = CPUTimer_getTimerCount(TIME_TIMER_BASE);
        periods++;
    }

    if (!wasDisabled)
    {
        (void)Interrupt_enableInProcessor();
    }

    return ((uint64_t)periods << 32) | (uint32_t)(0xFFFFFFFFU - count);
}

void board_init(void)
{
    /* Equivalent of C2000Ware's CM_init() (cm.c), for code in flash. */
    SysCtl_disableWatchdog();

    /* Flash wait states must be set from RAM: copy the RAM functions first. */
    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart, (size_t)&RamfuncsLoadSize);
    Flash_initModule(FLASH0CTRL_BASE, FLASH0ECC_BASE, DEVICE_FLASH_WAITSTATES);

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_UART0);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_ENET);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TIMER0);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TIMER1);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TIMER2);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_UDMA);

    /* vectorTableFlash: device/f2838x/cm/startup/startup_cm.c. */
    Interrupt_setVectorTableOffset((uint32_t)vectorTableFlash);

    initTimeBase();
}

void board_led_set(unsigned id, bool on)
{
    if (id < (sizeof(s_ledPins) / sizeof(s_ledPins[0])))
    {
        /* The LEDs are active low. */
        GPIO_writePin(s_ledPins[id], on ? 0U : 1U);
    }
}

void board_led_toggle(unsigned id)
{
    if (id < (sizeof(s_ledPins) / sizeof(s_ledPins[0])))
    {
        GPIO_togglePin(s_ledPins[id]);
    }
}

/* driverlib's ASSERT() hook (DEBUG builds). */
void __error__(const char* filename, uint32_t line)
{
    (void)filename;
    (void)line;
    __asm volatile("bkpt #0");
}
