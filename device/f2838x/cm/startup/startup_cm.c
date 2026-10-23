/*
 * Reset entry and exception vectors for F2838x Connectivity Manager,
 * TI Arm Clang, code in flash, FreeRTOS.
 *
 * Boot: CPU1 releases CM with BOOTMODE_BOOT_TO_FLASH_SECTOR0; CM boot ROM
 * then jumps to the start of flash sector 0, where the linker command file
 * places .resetisr (a branch to TI run-time start-up _c_int00). After C
 * initialization, board_init() points VTOR at vectorTableFlash.
 *
 * SVCall, PendSV and SysTick go straight to FreeRTOS port handlers.
 * Peripheral interrupts default to defaultISR; applications install their
 * handlers at run time with Interrupt_registerHandler(), which moves the table
 * to RAM (vectorTableRAM).
 *
 * Copyright 2026 Konstantin Chernyshov
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_types.h"

/* C-RAM, S-RAM and E0 RAM form one block: 0x1FFFC000 to 0x20013FFF. */
#define RAM_START                                                (0x1FFFC000UL)
#define RAM_END                                                  (0x20014000UL)
#define FRAME_WORDS                                                        (8U)

extern void _c_int00(void);
extern uint32_t __STACK_END;

extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);

static void nmiISR(void);
static void faultISR(void);
static void defaultISR(void);

volatile uint32_t g_startupFaultIpsr;

typedef struct
{
    uint32_t ipsr;          /* 3 HardFault, 4 MemManage, 5 BusFault, 6 UsageFault */
    uint32_t cfsr;          /* MMFSR | BFSR << 8 | UFSR << 16 */
    uint32_t hfsr;          /* bit 30: escalated fault; bit 31: bkpt without a debugger */
    uint32_t mmfar;         /* valid if CFSR bit 7 */
    uint32_t bfar;          /* valid if CFSR bit 15 */
    uint32_t excReturn;     /* bit 2 set: the frame is on the task stack (PSP) */
    uint32_t sp;            /* address of the exception frame */
    uint32_t r0, r1, r2, r3, r12;
    uint32_t lr;            /* return address of the code that faulted */
    uint32_t pc;            /* the faulting instruction (precise faults) */
    uint32_t xpsr;
} startup_fault_t;

volatile startup_fault_t g_startupFault;

__attribute__((section(".resetisr"), naked, used, retain))
void resetISR(void)
{
    __asm volatile("b.w _c_int00");
}

__attribute__((section(".vftable"), aligned(1024), used, retain))
void (* const vectorTableFlash[NUM_INTERRUPTS])(void) =
{
    [0]                          = (void (*)(void))&__STACK_END,
    [1]                          = resetISR,
    [FAULT_NMI]                  = nmiISR,
    [FAULT_HARD]                 = faultISR,
    [FAULT_MPU]                  = faultISR,
    [FAULT_BUS]                  = faultISR,
    [FAULT_USAGE]                = faultISR,
    [FAULT_SVCALL]               = vPortSVCHandler,
    [FAULT_DEBUG]                = defaultISR,
    [FAULT_PENDSV]               = xPortPendSVHandler,
    [FAULT_SYSTICK]              = xPortSysTickHandler,
    [16 ... NUM_INTERRUPTS - 1U] = defaultISR,
};

static void nmiISR(void)
{
    for(;;)
    {
    }
}

__attribute__((used, noreturn))
void startupFaultRecord(const uint32_t *frame, uint32_t excReturn)
{
    uint32_t ipsr;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));

    g_startupFaultIpsr = ipsr;
    g_startupFault.ipsr = ipsr;
    g_startupFault.cfsr = HWREG(NVIC_BASE + NVIC_O_CFSR);
    g_startupFault.hfsr = HWREG(NVIC_BASE + NVIC_O_HFSR);
    g_startupFault.mmfar = HWREG(NVIC_BASE + NVIC_O_MMFAR);
    g_startupFault.bfar = HWREG(NVIC_BASE + NVIC_O_BFAR);
    g_startupFault.excReturn = excReturn;
    g_startupFault.sp = (uint32_t)frame;

    /* A stack pointer outside RAM would fault again (lockup): skip the frame. */
    if (((uint32_t)frame >= RAM_START) &&
        ((uint32_t)frame <= (RAM_END - (FRAME_WORDS * sizeof(uint32_t)))))
    {
        g_startupFault.r0 = frame[0];
        g_startupFault.r1 = frame[1];
        g_startupFault.r2 = frame[2];
        g_startupFault.r3 = frame[3];
        g_startupFault.r12 = frame[4];
        g_startupFault.lr = frame[5];
        g_startupFault.pc = frame[6];
        g_startupFault.xpsr = frame[7];
    }

    for(;;)
    {
    }
}

__attribute__((naked))
static void faultISR(void)
{
    __asm volatile("tst   lr, #4\n"
                   "ite   eq\n"
                   "mrseq r0, msp\n"
                   "mrsne r0, psp\n"
                   "mov   r1, lr\n"
                   "b.w   startupFaultRecord\n");
}

static void defaultISR(void)
{
    __asm volatile("mrs %0, ipsr" : "=r"(g_startupFaultIpsr));
    for(;;)
    {
    }
}
